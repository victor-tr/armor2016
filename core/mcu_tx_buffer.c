#include "ql_uart.h"

#include "mcu_tx_buffer.h"

#include "service/crc.h"

#include "core/canbus.h"
#include "core/timers.h"

#include "db/db.h"

#include "common/errors.h"

#include "service/humanize.h"


REGISTER_FIFO_C(TxMcuMessage, mcu_tx_buffer, NULL, NULL)

static bool _b_wait_for_ACK_from_MCU = FALSE;


static s32 dispatch_msg_for_systemboard(void)
{
    OUT_DEBUG_2("dispatch_msg_for_systemboard()\r\n");

    TxMcuMessage msg;

    s32 ret = mcu_tx_buffer.dequeue(&msg);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("mcu_tx_buffer::dequeue() = %d error\r\n", ret);
        return ret;
    }

    ret = Ql_UART_Write(UART_PORT3, msg.data, msg.len);
    if (ret < 0) {
        OUT_DEBUG_1("Ql_UART_Write(UART_PORT3) = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

static s32 send_to_systemboard(UnitIdent *unit, McuCommandCode cmd, u8 *data, u16 datalen)
{
    OUT_DEBUG_2("send_to_systemboard()\r\n");

    u16 len = MCUPKT_BEGIN_DATABLOCK + datalen + 1;  // MCUPKT_ prefix + datalen + CRC

    TxMcuMessage msg;

    msg.data[MCUPKT_MARKER] = MCU_PACKET_MARKER;
    msg.data[MCUPKT_PARENTDEV_UIN_H] = unit->uin >> 8;
    msg.data[MCUPKT_PARENTDEV_UIN_L] = unit->uin;
    msg.data[MCUPKT_PARENTDEV_TYPE] = unit->type;
    msg.data[MCUPKT_PKT_NUMBER] = 0x80;
    msg.data[MCUPKT_DATABLOCK_LENGTH] = datalen + 3; // 3 bytes: Cmd, SUIN, SType
    msg.data[MCUPKT_COMMAND_CODE] = cmd;
    msg.data[MCUPKT_SUBDEV_UIN] = unit->suin;
    msg.data[MCUPKT_SUBDEV_TYPE] = unit->stype;

    for (u16 i = 0; i < datalen; ++i) {
        msg.data[MCUPKT_BEGIN_DATABLOCK + i] = data ? data[i] : 0xFF;
    }

    msg.data[len-1] = evaluateMcuCRC(msg.data);
    msg.len = len;

    s32 ret = mcu_tx_buffer.enqueue(&msg);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("mcu_tx_buffer::enqueue() = %d error\r\n", ret);
        return ret;
    }

    ret = process_mcu_tx_buffer();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("process_mcu_tx_buffer() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

void process_received_mcu_ACK(void)
{
    Ar_Timer_stop(TMR_McuCtsFlag_Timer);
#ifndef NO_CB

    OUT_DEBUG_2("process_received_mcu_ACK()\r\n");

#endif

    _b_wait_for_ACK_from_MCU = FALSE;

    MasterBoard *pMasterboard = &getCommonSettings(NULL)->p_masterboard->d.masterboard;
    if (pMasterboard->bMcuConnectionRequested) {
        pMasterboard->bMcuConnectionRequested = FALSE;
        pMasterboard->bMcuApproved = TRUE;
    }

    s32 ret = process_mcu_tx_buffer();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("process_mcu_tx_buffer() = %d error\r\n", ret);
    }
}

s32 process_mcu_tx_buffer(void)
{
    bool bMcuApproved = getCommonSettings(NULL)->p_masterboard->d.masterboard.bMcuApproved;

#ifndef NO_CB

    OUT_DEBUG_2("process_mcu_tx_buffer(MCU ready=%s, MCU approved=%s, CAN ready=%s)\r\n",
                !_b_wait_for_ACK_from_MCU ? "YES" : "NO",
                bMcuApproved ? "YES" : "NO",
                Ar_CAN_isTxBufferReadyToSend() ? "YES" : "NO");

#endif

    if (_b_wait_for_ACK_from_MCU)
        return RETURN_NO_ERRORS;

    bool sent = FALSE;

    if (Ar_CAN_isTxBufferReadyToSend() && bMcuApproved)
    {
        s32 ret = Ar_CAN_send_message(); // send CAN pkt
        if (ERR_BUFFER_IS_EMPTY == ret) {
#ifndef NO_CB

            OUT_DEBUG_3("Ar_CAN_send_message(): CAN buffer is empty\r\n");

#endif

            return RETURN_NO_ERRORS;
        } else if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Ar_CAN_send_message() = %d error\r\n", ret);
            return ret;
        }
        sent = TRUE;
    }
    else if (!mcu_tx_buffer.isEmpty())
    {
        s32 ret = dispatch_msg_for_systemboard();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("dispatch_msg_for_systemboard() = %d error\r\n", ret);
            return ret;
        }
        sent = TRUE;
    }

    if (sent) {
        _b_wait_for_ACK_from_MCU = TRUE;
        Ar_Timer_startSingle(TMR_McuCtsFlag_Timer, 200);
    }
    return RETURN_NO_ERRORS;
}

s32 sendToMCU(UnitIdent *unit, McuCommandCode cmd, u8 *data, u16 datalen)
{
    OUT_DEBUG_2("sendToMCU(): -> %s #%d [%s #%d]\r\n",
                getUnitTypeByCode(unit->type), unit->uin, getUnitTypeByCode(unit->stype), unit->suin);

    s32 ret = 0;

    switch ((u16)unit->type) {
    case OBJ_MASTERBOARD:
    {
        ret = send_to_systemboard(unit, cmd, data, datalen);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("send_to_systemboard() = %d error\r\n", ret);
            return ret;
        }
        break;
    }

    case 0:             // send broadcast
    case OBJ_ETR:
    case OBJ_RZE:
    case OBJ_RAE:
    case OBJ_KEYBOARD:
    {
        if (CMD_DenyConnectionRequest != cmd &&
                !isParentDeviceConnected(unit->uin, unit->type))
        {
            OUT_DEBUG_3("Unit %s #%d is not connected\r\n",
                        getUnitTypeByCode(unit->type), unit->uin);
            return RETURN_NO_ERRORS;
        }

        ret = Ar_CAN_prepare_message_to_send(unit, cmd, data, datalen);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Ar_CAN_prepare_message_to_send() = %d error\r\n", ret);
            return ret;
        }
        break;
    }

    default:
        break;
    }

    return RETURN_NO_ERRORS;
}

s32 sendToMCU_2(McuCommandRequest *pRequest)
{
    UnitIdent *unit = &pRequest->unit;
    McuCommandCode cmd = pRequest->cmd;
    u8 *data = pRequest->data;
    u16 datalen = pRequest->datalen;

    OUT_DEBUG_2("sendToMCU_2(): -> %s #%d [%s #%d]\r\n",
                getUnitTypeByCode(unit->type), unit->uin, getUnitTypeByCode(unit->stype), unit->suin);

    s32 ret = 0;

    switch ((u16)unit->type) {
    case OBJ_MASTERBOARD:
    {
        ret = send_to_systemboard(unit, cmd, data, datalen);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("send_to_systemboard() = %d error\r\n", ret);
            return ret;
        }
        break;
    }

    case 0:             // send broadcast
    case OBJ_ETR:
    case OBJ_RZE:
    case OBJ_RAE:
    case OBJ_KEYBOARD:
    {
        if (CMD_DenyConnectionRequest != cmd &&
                !isParentDeviceConnected(unit->uin, unit->type))
        {
            OUT_DEBUG_3("Unit %s #%d is not connected\r\n",
                        getUnitTypeByCode(unit->type), unit->uin);
            return RETURN_NO_ERRORS;
        }

        ret = Ar_CAN_prepare_message_to_send(unit, cmd, data, datalen);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Ar_CAN_prepare_message_to_send() = %d error\r\n", ret);
            return ret;
        }
        break;
    }

    default:
        break;
    }

    return RETURN_NO_ERRORS;
}

void mcu_cts_flag_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_McuCtsFlag_Timer));
    process_received_mcu_ACK();
}
