#include "canbus.h"

#include "ql_memory.h"
#include "ql_uart.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "service/crc.h"

#include "core/mcu_tx_buffer.h"
#include "core/uart.h"
#include "core/timers.h"

#include "pult/pult_tx_buffer.h"


#define CANBUS_FREEZE_THRESHOLD 5


static PeripheryMessageBuffer _rx_buffer;
static PeripheryMessageBuffer _tx_buffer;

static CAN_TxBufferState _tx_buffer_state = CAN_Tx_Standby;
static bool _b_sending_in_progress = FALSE;
static u8 _freeze_counter = 0;
static u8 _whole_msg_rx_buffer[CAN_MESSAGE_BUFFER_SIZE];

static PeripheryMsgHandler _periphery_msg_handler_callback = NULL;


//**********************************************************************************
//  Prototypes
//**********************************************************************************
static PeripheryMessage *CAN_find_message(PeripheryMessageBuffer * const buffer,
                                          UnitIdent *unit,
                                          McuCommandCode cmd);
static s32 CAN_register_new_message(PeripheryMessageBuffer * const buffer,
                                    PeripheryMessage **message,
                                    UnitIdent *unit,
                                    McuCommandCode cmd);
static s32 CAN_clear_all_msg_chunks(PeripheryMessage *message);
static s32 CAN_remove_message(PeripheryMessageBuffer * const buffer,
                              PeripheryMessage *message);
static s32 CAN_reconstruct_received_message(PeripheryMessage *message);
static s32 CAN_append_msg_chunk(u8 *data, u8 datalen, u8 CAN_pkt_id, PeripheryMessage *pMsg);


//**********************************************************************************
//  Definitions
//**********************************************************************************
void Ar_CAN_processSendingResult(CAN_TxBufferState result)
{
    _tx_buffer_state = result;

    if (Ar_CAN_isTxBufferReadyToSend())
    {
#ifndef NO_CB

        OUT_DEBUG_3("Set CAN RTS flag\r\n");

#endif

        if (_freeze_counter >= CANBUS_FREEZE_THRESHOLD) {
            s32 ret = reportToPult(0,0, PMC_CANBusFreeze, PMQ_RestoralOrSetArmed);
            if (ret < RETURN_NO_ERRORS)
                OUT_DEBUG_1("Ar_CAN_processSendingResult: reportToPult() = %d error.\r\n", ret);
        }

        _freeze_counter = 0;

        s32 ret = process_mcu_tx_buffer();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("process_mcu_tx_buffer() = %d error.\r\n", ret);
        }
    }
}

bool Ar_CAN_isTxBufferReadyToSend(void)
{
    return (CAN_Tx_ReadyToRepeat == _tx_buffer_state ||
               CAN_Tx_ReadyToSend == _tx_buffer_state);
}

// -- the function below should be called one time per second
void Ar_CAN_checkBusActivity(void)
{
    if (_freeze_counter && _freeze_counter < CANBUS_FREEZE_THRESHOLD)
    {
        ++_freeze_counter;

        if (_freeze_counter < CANBUS_FREEZE_THRESHOLD) { // checking attempts threshold
            OUT_DEBUG_3("CAN Tx buffer freeze\r\n");

            CREATE_UNIT_IDENT(u,
                              OBJ_MASTERBOARD, getCommonSettings(NULL)->p_masterboard->uin,
                              OBJ_CAN_CONTROLLER, 0x01);

            s32 ret = sendToMCU(&u, CMD_GetValue, 0, 0);
            if (ret < RETURN_NO_ERRORS)
                OUT_DEBUG_1("Ar_CAN_checkBusActivity(): sendToMCU() = %d error\r\n", ret);
        }
        else {
            s32 ret = reportToPult(0,0, PMC_CANBusFreeze, PMQ_NewMsgOrSetDisarmed);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("Ar_CAN_checkBusActivity(): reportToPult() = %d error\r\n", ret);
                return;
            }
        }
    }
}

void Ar_CAN_registerMessageHandler(PeripheryMsgHandler handler_callback)
{
    OUT_DEBUG_2("Ar_CAN_registerMessageHandler()\r\n");
    _periphery_msg_handler_callback = handler_callback;
}

s32 Ar_CAN_saveReceivedPkt(u8 *pkt)
{
#ifndef NO_CB

        OUT_DEBUG_2("Ar_CAN_saveReceivedPkt()\r\n");

#endif


    if (!_periphery_msg_handler_callback)
        return ERR_CANBUS_MSG_HANDLER_NOT_REGISTERED;

    s32 ret = 0;

    UnitIdent unit = getUnitIdent(pkt);
    McuCommandCode cmd = (McuCommandCode)pkt[MCUPKT_COMMAND_CODE];

    PeripheryMessage *pMsg = CAN_find_message(&_rx_buffer, &unit, cmd);
    if (!pMsg) {
        ret = CAN_register_new_message(&_rx_buffer, &pMsg, &unit, cmd);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("CAN_register_new_message() = %d error\r\n", ret);
            return ret;
        }
    }


    // -- clear garbage if needed
    u8 current_pkt_id = pkt[MCUPKT_PKT_NUMBER] & 0x7F;

#ifndef NO_CB

    OUT_DEBUG_3("last_recv_id: %d, current_recv_id: %d\r\n", pMsg->last_pkt_id, current_pkt_id);

#endif


    if (pMsg->last_pkt_id + 1 != current_pkt_id)
    {
        if (1 == current_pkt_id) {
            OUT_DEBUG_1("Previous peripheral message is corrupted. It will be replaced.\r\n");

            ret = CAN_clear_all_msg_chunks(pMsg);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("CAN_clear_all_msg_chunks() = %d error\r\n", ret);
                return ret;
            }
        } else {
            OUT_DEBUG_1("The peripheral message is corrupted. It will be removed.\r\n");

            ret = CAN_remove_message(&_rx_buffer, pMsg);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("CAN_remove_message() = %d error\r\n", ret);
                return ret;
            }
            return RETURN_NO_ERRORS;
        }
    }

    // --
    ret = CAN_append_msg_chunk(&pkt[MCUPKT_BEGIN_DATABLOCK],
                           pkt[MCUPKT_DATABLOCK_LENGTH] - 3,  // 3 bytes: Cmd, SUIN, SType
                           current_pkt_id,
                           pMsg);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CAN_append_msg_chunk() = %d error\r\n", ret);
        return ret;
    }

    // -- if message is complete => immediately process it
    if (pkt[MCUPKT_PKT_NUMBER] & 0x80) {
        ret = CAN_reconstruct_received_message(pMsg);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("CAN_reconstruct_received_message() = %d error\r\n", ret);
            return ret;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 Ar_CAN_prepare_message_to_send(UnitIdent *pUnit, McuCommandCode cmd, u8 *data, u16 datalen)
{
    OUT_DEBUG_2("Ar_CAN_prepare_message_to_send()\r\n");

    u8 pkt_qty = (datalen % CAN_DATA_BYTES_PER_PKT) || !datalen
                        ? datalen / CAN_DATA_BYTES_PER_PKT + 1
                        : datalen / CAN_DATA_BYTES_PER_PKT;

    PeripheryMessage *pMsg = NULL;
    s32 ret = CAN_register_new_message(&_tx_buffer, &pMsg, pUnit, cmd);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CAN_register_new_message() = %d error\r\n", ret);
        return ret;
    }

    for (u8 i = 0; i < pkt_qty; ++i) {
        /*
         * if not the last pkt in the chain OR if the last
         *  pkt has "CAN_DATA_BYTES_PER_PKT" data bytes too
         */
        u8 pkt_len = (i + 1 != pkt_qty) ||
                            (!(datalen % CAN_DATA_BYTES_PER_PKT) && datalen)
                        ? CAN_DATA_BYTES_PER_PKT
                        : datalen % CAN_DATA_BYTES_PER_PKT;

        u8 *pkt = datalen ? &data[i * CAN_DATA_BYTES_PER_PKT] : NULL;

        ret = CAN_append_msg_chunk(pkt,
                               pkt_len,
                               1,       // 1 - not sent any pkt yet, just prepare
                               pMsg);
    }


    if (CAN_Tx_Standby == _tx_buffer_state) {  // only if the buffer is NOT processed now
        _tx_buffer_state = CAN_Tx_ReadyToSend;
    }
    else if (CAN_Tx_WaitForCANACK == _tx_buffer_state) {
        if (!_freeze_counter)
            _freeze_counter = 1;
    }

    ret = process_mcu_tx_buffer();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("process_mcu_tx_buffer() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 Ar_CAN_send_message(void)
{
    static CANPkt *current_pkt = NULL;
    static PeripheryMessage *current_msg = NULL;

#ifndef NO_CB

    OUT_DEBUG_2("Ar_CAN_send_message(already_in_progress=%s)\r\n",
                _b_sending_in_progress ? "TRUE" : "FALSE");

#endif

    while (1) {     // get message and packet
        // first msg check
        if (!current_msg)
            current_msg = _tx_buffer.pFirstMsg;

        // second msg check
        if (!current_msg) {
            _b_sending_in_progress = FALSE;
            _tx_buffer_state = CAN_Tx_Standby;
            return ERR_BUFFER_IS_EMPTY;
        }

        // first pkt check
        if (!current_pkt) {
            current_pkt = current_msg->pkt_chain_head;
        } else {
            if (CAN_Tx_ReadyToSend == _tx_buffer_state) {
                current_pkt = current_pkt->pNext;
                current_msg->last_pkt_id++;
            }
        }

        // second pkt check
        if (current_pkt) {
            _b_sending_in_progress = TRUE;
            break;
        }

        CAN_remove_message(&_tx_buffer, current_msg);
        current_msg = NULL;
        current_pkt = NULL;
    }

    u8 buf[MCUPKT_BEGIN_DATABLOCK + CAN_DATA_BYTES_PER_PKT + 1] = {0};  // MCUPKT_ prefix + datalen + CRC

    buf[MCUPKT_MARKER] = MCU_PACKET_MARKER;
    buf[MCUPKT_PARENTDEV_UIN_H] = current_msg->unit.uin >> 8;
    buf[MCUPKT_PARENTDEV_UIN_L] = current_msg->unit.uin;
    buf[MCUPKT_PARENTDEV_TYPE] = current_msg->unit.type;

    u8 last_pkt_bitmask = (current_pkt == current_msg->pkt_chain_tail) ? 0x80 : 0x00;
    buf[MCUPKT_PKT_NUMBER] = last_pkt_bitmask | current_msg->last_pkt_id;

    u8 bodylen = current_pkt ? current_pkt->len : 0;
    buf[MCUPKT_DATABLOCK_LENGTH] = bodylen + 3; // 3 bytes: Cmd, SUIN, SType

    buf[MCUPKT_COMMAND_CODE] = current_msg->command;
    buf[MCUPKT_SUBDEV_UIN] = current_msg->unit.suin;
    buf[MCUPKT_SUBDEV_TYPE] = current_msg->unit.stype;

    for (u8 i = 0; i < bodylen; ++i)
        buf[MCUPKT_BEGIN_DATABLOCK + i] = current_pkt->data[i];

    buf[MCUPKT_BEGIN_DATABLOCK + bodylen] = evaluateMcuCRC(buf);

    s32 ret = Ql_UART_Write(UART_PORT3, buf, MCUPKT_BEGIN_DATABLOCK + bodylen + 1);
    if (ret < 0) {
        OUT_DEBUG_1("Ql_UART_Write(UART_PORT3) = %d error\r\n", ret);
        CAN_remove_message(&_tx_buffer, current_msg);
        current_msg = NULL;
        current_pkt = NULL;
        return ret;
    }

    _tx_buffer_state = CAN_Tx_WaitForCANACK;
    return RETURN_NO_ERRORS;
}



// -- buffer service functions
PeripheryMessage *CAN_find_message(PeripheryMessageBuffer * const buffer,
                                   UnitIdent *unit,
                                   McuCommandCode cmd)
{
    PeripheryMessage *message = buffer->pFirstMsg;

    while (message) {
        if (message->unit.suin == unit->suin &&
                message->unit.stype == unit->stype &&
                message->unit.uin == unit->uin &&
                message->unit.type == unit->type &&
                message->command == cmd)
            return message;

        message = buffer->pFirstMsg->pNext;
    }

#ifndef NO_CB

    OUT_DEBUG_3("Periphery message was not found\r\n");

#endif
    return NULL;
}

s32 CAN_clear_all_msg_chunks(PeripheryMessage *message)
{
#ifndef NO_CB

    OUT_DEBUG_2("CAN_clear_all_msg_chunks()\r\n");

#endif

    while (message->pkt_chain_head) {
        CANPkt *temp = message->pkt_chain_head->pNext;
        Ql_MEM_Free(message->pkt_chain_head);
        message->pkt_chain_head = temp;
    }

    message->total_data_len = 0;
    message->last_pkt_id = 0;
    message->pkt_chain_head = NULL;
    message->pkt_chain_tail = NULL;

    return RETURN_NO_ERRORS;
}

s32 CAN_remove_message(PeripheryMessageBuffer * const buffer,
                       PeripheryMessage *message)
{
#ifndef NO_CB

    OUT_DEBUG_2("CAN_remove_message()\r\n");

#endif

    if (buffer && message) {
        s32 ret = CAN_clear_all_msg_chunks(message);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("CAN_clear_all_msg_chunks() = %d error\r\n", ret);
            return ret;
        }

        // -- remove message item
        if (message->pPrev) // if not first message in the buffer
            message->pPrev->pNext = message->pNext;
        else                // if first
            buffer->pFirstMsg = message->pNext;

        if (message->pNext) // if not last
            message->pNext->pPrev = message->pPrev;
        else                // if last
            buffer->pLastMsg = message->pPrev;

        buffer->buffer_size--;

        Ql_MEM_Free(message);
    }

    return RETURN_NO_ERRORS;
}

s32 CAN_register_new_message(PeripheryMessageBuffer * const buffer,
                             PeripheryMessage **message,
                             UnitIdent *unit,
                             McuCommandCode cmd)
{

#ifndef NO_CB

    OUT_DEBUG_2("CAN_register_new_message()\r\n");

#endif

    if (!buffer || !unit || *message)
        return RETURN_NO_ERRORS;

    // -- get memory
    PeripheryMessage *pNewMessage = Ql_MEM_Alloc(sizeof(PeripheryMessage));
    if (!pNewMessage) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", sizeof(PeripheryMessage));
        *message = NULL;
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    // -- fill the fields
    pNewMessage->pPrev = buffer->pLastMsg;
    pNewMessage->pNext = NULL;

    pNewMessage->unit = *unit;
    pNewMessage->command = cmd;

    pNewMessage->total_data_len = 0;
    pNewMessage->last_pkt_id = 0;
    pNewMessage->pkt_chain_head = NULL;
    pNewMessage->pkt_chain_tail = NULL;

    // -- append to the chain
    if (buffer->pLastMsg)
        buffer->pLastMsg->pNext = pNewMessage;
    else
        buffer->pFirstMsg = pNewMessage;

    buffer->pLastMsg = pNewMessage;
    buffer->buffer_size++;


    *message = pNewMessage;

    return RETURN_NO_ERRORS;
}

s32 CAN_reconstruct_received_message(PeripheryMessage *message)
{

#ifndef NO_CB

    OUT_DEBUG_2("CAN_reconstruct_received_message()\r\n");

#endif

    _whole_msg_rx_buffer[MCUPKT_MARKER] = MCU_PACKET_MARKER;
    _whole_msg_rx_buffer[MCUPKT_PARENTDEV_UIN_H] = message->unit.uin >> 8;
    _whole_msg_rx_buffer[MCUPKT_PARENTDEV_UIN_L] = message->unit.uin;
    _whole_msg_rx_buffer[MCUPKT_PARENTDEV_TYPE] = message->unit.type;
    _whole_msg_rx_buffer[MCUPKT_PKT_NUMBER] = 0x80;
    _whole_msg_rx_buffer[MCUPKT_DATABLOCK_LENGTH] = message->total_data_len + 3; // 3 bytes: Cmd, SUIN, SType
    _whole_msg_rx_buffer[MCUPKT_COMMAND_CODE] = message->command;
    _whole_msg_rx_buffer[MCUPKT_SUBDEV_UIN] = message->unit.suin;
    _whole_msg_rx_buffer[MCUPKT_SUBDEV_TYPE] = message->unit.stype;

    CANPkt *temp = message->pkt_chain_head;

    for (u16 i = MCUPKT_BEGIN_DATABLOCK;
            i < MCUPKT_BEGIN_DATABLOCK + message->total_data_len;
            i += CAN_DATA_BYTES_PER_PKT)
    {
        for (u8 j = 0; j < CAN_DATA_BYTES_PER_PKT; ++j)
            _whole_msg_rx_buffer[i + j] = temp->data[j];

        temp = temp->pNext;
    }

    //_buffer[MCUPKT_BEGIN_DATABLOCK + message->data_bytes_qty] = evaluateMcuCRC(_buffer);

    s32 ret = _periphery_msg_handler_callback(_whole_msg_rx_buffer);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("_periphery_msg_handler_callback() = %d error\r\n", ret);
    }

    return CAN_remove_message(&_rx_buffer, message);
}

s32 CAN_append_msg_chunk(u8 *data, u8 datalen, u8 CAN_pkt_id, PeripheryMessage *pMsg)
{

#ifndef NO_CB

    OUT_DEBUG_2("CAN_append_msg_chunk()\r\n");

#endif

    // -- create new chain's item
    CANPkt *pNewPkt = Ql_MEM_Alloc(sizeof(CANPkt));
    if (!pNewPkt) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", sizeof(CANPkt));
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    pNewPkt->len = datalen;
    pNewPkt->pNext = NULL;

    for (u8 i = 0; i < sizeof(pNewPkt->data); ++i)
        pNewPkt->data[i] = i < pNewPkt->len ? data[i] : 0;


    // -- append to the chain
    if (pMsg->pkt_chain_tail)
        pMsg->pkt_chain_tail->pNext = pNewPkt;
    else
        pMsg->pkt_chain_head = pNewPkt;

    pMsg->pkt_chain_tail = pNewPkt;
    pMsg->total_data_len += pNewPkt->len;
    pMsg->last_pkt_id = CAN_pkt_id;

    return RETURN_NO_ERRORS;
}
