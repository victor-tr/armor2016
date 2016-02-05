/*
 * File:    uart_transport.c
 *
 * Created: 15.10.2012 13:15:38
 * Author:  remicollab@gmail.com
 */

#include "core/uart.h"
#include "core/simcard.h"
#include "core/system.h"
#include "core/canbus.h"
#include "core/power.h"
#include "core/mcudefinitions.h"
#include "core/mcu_tx_buffer.h"
#include "core/mcu_rx_buffer.h"
#include "core/atcommand.h"

#include "common/errors.h"
#include "common/debug_macro.h"
#include "common/defines.h"

#include "service/crc.h"

#include "states/armingstates.h"

#include "configurator/configurator.h"
#include "configurator/configurator_rx_buffer.h"

#include "db/lexicon.h"

#include "events/event_zone.h"
#include "events/event_key.h"
#include "events/event_button.h"
#include "events/event_internal_timer.h"


extern s32 Ql_RIL_IsURCStr(const char* strRsp);


static u8 _mcu_rx_temp_buffer[MCU_MESSAGE_MAX_SIZE];
static u8 _configurator_rx_temp_buffer[CONFIGURATOR_MESSAGE_MAX_SIZE];

/* local functions */
static s32 parseMainRxUartData (u8 *pData, s16 len);
static s32 parseDebugRxUartData(u8 *pData, s16 len);
static s32 parseAuxRxUartData  (u8 *pData, s16 len);
static bool is_AT_command(u8 *data);


/* returns totally read bytes */
static s32 getRxUartData(Enum_SerialPort port, u8 *data, const u32 len)
{
    if (NULL == data || 0 == len)
        return ERR_INVALID_PARAMETER;

    s32 total_read = 0;
    s32 read = 0;

    //Ql_memset(data, 0, len);    // cleanup the buffer

    while (1) {
        read = Ql_UART_Read(port, data, len - total_read);
        if (read <= 0) break;   // all data is read out, or uart error
        total_read += read;
    }
    if (read < 0) {     // uart reading error
        OUT_DEBUG_1("Ql_UART_Read(port=%d) = %d error\r\n", port, read);
        Ql_UART_ClrRxBuffer(port);
        return ERR_CANNOT_READ_RX_UART_DATA;
    }

    /* terminate the data in the case of C-string */
    u32 terminator_pos = total_read < len ? total_read : len;
    data[terminator_pos] = 0;

    return total_read;
}


UnitIdent getUnitIdent(u8 *mcuPkt)
{
    UnitIdent u;
    u.uin = mcuPkt[MCUPKT_PARENTDEV_UIN_H] << 8 | mcuPkt[MCUPKT_PARENTDEV_UIN_L];
    u.type = (DbObjectCode)mcuPkt[MCUPKT_PARENTDEV_TYPE];
    u.suin = mcuPkt[MCUPKT_SUBDEV_UIN];
    u.stype = (DbObjectCode)mcuPkt[MCUPKT_SUBDEV_TYPE];
    return u;
}

s32 parseMainRxUartData(u8 *pData, s16 len)
{
#ifndef NO_CB

    OUT_UNPREFIXED_LINE("\r\n");

#endif

    if (!configurator_rx_circular_buffer.enqueue(pData, len)) {
        OUT_DEBUG_1("Too many data: the message will be lost\r\n");
        OUT_UNPREFIXED_LINE("\r\n");
        return RETURN_NO_ERRORS;
    }

    s32 ret = 0;
    u16 pktlen = 0;
    u8 *buf = _configurator_rx_temp_buffer;
    u16 counter = 1;

    while (1)
    {
        // -- try to read a pkt
        const s16 size_before = configurator_rx_circular_buffer.size();
        if (size_before < SPS_PREF_N_SUFF)
            break;

        if (!configurator_rx_circular_buffer.readPkt(buf, &pktlen))
        {
            if (size_before == configurator_rx_circular_buffer.size())
                break;
            else
                continue;
        }

        // data from a PC-configurator
        if (CONFIGURATION_PACKET_MARKER == buf[SPS_MARKER] && checkConfiguratorCRC(buf))
        {
            OUT_DEBUG_2("parseMainRxUartData(n=%d, len_buffered=%d, len_pkt=%d)\r\n",
                        counter,
                        configurator_rx_circular_buffer.size(),
                        pktlen);

            RxConfiguratorPkt pkt;

            pkt.bAppend =       buf[SPS_APPEND_FLAG];
            pkt.data =         &buf[SPS_PREFIX];
            pkt.datalen =       buf[SPS_BYTES_QTY_H] << 8 | buf[SPS_BYTES_QTY_L];
            pkt.dataType =      (DbObjectCode)buf[SPS_FILECODE];
            pkt.marker =        buf[SPS_MARKER];
            pkt.pktIdx =        buf[SPS_PKT_INDEX];
            pkt.repeatCounter = buf[SPS_PKT_REPEAT];
            pkt.sessionKey =    buf[SPS_PKT_KEY];

            ret = Ar_Configurator_processSettingsData(&pkt, CCM_DirectConnection);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("Ar_Configurator_processSettingsData() = %d error.\r\n", ret);
            }

            // -- go to next settings packet if any
            configurator_rx_circular_buffer.skip(pktlen);
            counter++;
        }
        else
        {
            configurator_rx_circular_buffer.skip(1);
        }
    }

    return RETURN_NO_ERRORS;
}

s32 parseDebugRxUartData(u8 *pData, s16 len)
{
//    OUT_UNPREFIXED_LINE("\r\n")
//    OUT_DEBUG_2("parseDebugRxUartData()\r\n");

    return RETURN_NO_ERRORS;
}

s32 parseAuxRxUartData(u8 *pData, s16 len)
{
#ifndef NO_CB

    OUT_UNPREFIXED_LINE("\r\n");

#endif

    if (!mcu_rx_circular_buffer.enqueue(pData, len)) {
        OUT_DEBUG_1("Too many data: MCU message will be lost\r\n");
        OUT_UNPREFIXED_LINE("\r\n");
        return RETURN_NO_ERRORS;
    }

    s32 ret = 0;
    u8 *mcuPkt = _mcu_rx_temp_buffer;
    u16 counter = 1;
    u16 pktlen = 0;

    while (1)
    {
        // -- try to read a pkt
        const s16 size_before = mcu_rx_circular_buffer.size();
        if (size_before < MCUPKT_MIN_PKT_LEN)
            break;

        if (!mcu_rx_circular_buffer.readPkt(mcuPkt, &pktlen))
        {
            if (size_before == mcu_rx_circular_buffer.size())
                break;
            else
                continue;
        }

        // -- if correct data packet recveived from MCU
        if (MCU_PACKET_MARKER == mcuPkt[MCUPKT_MARKER] && checkMcuCRC(mcuPkt))
        {

#ifndef NO_CB

            OUT_DEBUG_2("parseAuxRxUartData(n=%d, len_buffered=%d, len_pkt=%d)\r\n",
                        counter,
                        mcu_rx_circular_buffer.size(),
                        pktlen);

#endif

            RxMcuMessage mcumsg;

            mcumsg.marker  = mcuPkt[MCUPKT_MARKER];
            mcumsg.unit    = getUnitIdent(mcuPkt);
            mcumsg.cmd     = (McuCommandCode)mcuPkt[MCUPKT_COMMAND_CODE];
            mcumsg.data    = &mcuPkt[MCUPKT_BEGIN_DATABLOCK];
            mcumsg.datalen = mcuPkt[MCUPKT_DATABLOCK_LENGTH];
            mcumsg.pkt_id  = mcuPkt[MCUPKT_PKT_NUMBER];


            Ar_System_markUnitAsConnected(&mcumsg.unit);

            // ===========================================================================
            // -- section 1: there is no difference whether MCU is connected or not
            if ( CMD_ConnectionRequest == mcumsg.cmd
                    && OBJ_MASTERBOARD == mcumsg.unit.type
                    && OBJ_AUX_MCU == mcumsg.unit.stype )
            {
                updateMasterboardUin(mcumsg.unit.uin);
//DBRestoreLedMessage(mcumsg.unit.uin);

                /* -- note: MCU doesn't send MSG_PackReceived when MCU restart is requested,
                 *          but send CMD_ConnectionRequest instead,
                 *          so we need to simulate the ACK was received before any other actions
                 */
                process_received_mcu_ACK();

                // --
                MasterBoard *pMasterboard = &getCommonSettings(NULL)->p_masterboard->d.masterboard;
                pMasterboard->bMcuApproved = FALSE;
                pMasterboard->bMcuConnectionRequested = TRUE;

                // -- enable AuxMCU
                if(!Ar_System_isFOTA_process_flag())
                {
                ret = Ar_System_acceptUnitConnectionRequest(&mcumsg.unit, TRUE);
                if (ret < RETURN_NO_ERRORS)
                    OUT_DEBUG_1("Ar_System_acceptUnitConnectionRequest() = %d error\r\n", ret);

                // -- restart all periphery devices
                UnitIdent u = {0};
                ret = sendToMCU(&u, CMD_Restart, 0, 0);

                if (ret < RETURN_NO_ERRORS){
                    OUT_DEBUG_1("sendToMCU() = %d error\r\n", ret);
                }
                else
                {
                    OUT_DEBUG_1("Restart all periphery devices\r\n");
                }

                // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                // -- disable system bell's autoreport
                UnitIdent bellUnit = {0};
                bellUnit.stype = OBJ_BELL;
                bellUnit.suin = 0x01;
                bellUnit.type = mcumsg.unit.type;
                bellUnit.uin = mcumsg.unit.uin;
                Ar_System_enableBellAutoreport(&bellUnit, TRUE);
                // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                }
                else
                {
                    ret = Ar_System_acceptUnitConnectionRequest(&mcumsg.unit, FALSE);
                    if (ret < RETURN_NO_ERRORS)
                        OUT_DEBUG_1("Ar_System_acceptUnitConnectionRequest() = %d error\r\n", ret);
                }

            }

            // -- MCU handled previous packet
            else if (MSG_PackReceived == mcumsg.cmd) {
                process_received_mcu_ACK();
            }

            // -- MCU status => send one beep to the main bell
            else if (MSG_GetStatus == mcumsg.cmd) {
                Ar_System_errorBeepEtrBuzzer(2, 1, 1);  // one beep
            }

            // -- process power message
            else if (OBJ_MASTERBOARD == mcumsg.unit.type
                     &&  OBJ_Source == mcumsg.unit.stype)
            {
                ret = Ar_Power_processPowerSourceMsg(mcuPkt);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("Ar_Power_processPowerSourceMsg() = %d error.\r\n", ret);
                }

            }
            // ===========================================================================
            // -- section 2: don't handle any events from the MCU if GSM-module has not been initialized yet,
            else if (!Ar_System_checkFlag(SysF_CanHandleMcuMessages)) {
                OUT_DEBUG_3("The device is not initialized normally. "
                            "Received event will be ignored.\r\n");
            }

            // ===========================================================================
            // -- section 3: handle other cases
            else
            {
                switch (mcumsg.unit.type) {
                case OBJ_MASTERBOARD:
                {
                    switch (mcumsg.unit.stype) {
                    case OBJ_ZONE:
                    {
                        ret = process_mcu_zone_msg(mcuPkt);
                        if (ret < RETURN_NO_ERRORS) {
                            OUT_DEBUG_1("process_mcu_zone_msg() = %d error.\r\n", ret);
                        }
                        break;
                    }

                    case OBJ_CAN_CONTROLLER:
                    {
                        const char *recv_lex = (char *)(mcumsg.data + 1);
                        if (Ar_Lexicon_isEqual(recv_lex, lexicon.CAN.out.EvtDone))
                        {
                            Ar_CAN_processSendingResult(CAN_Tx_ReadyToSend);
                        }
                        else if (Ar_Lexicon_isEqual(recv_lex, lexicon.CAN.out.EvtBusy))
                        {
                            OUT_DEBUG_3("CAN controller is BUSY. Do nothing.\r\n");
                        }
                        else if (Ar_Lexicon_isEqual(recv_lex, lexicon.CAN.out.EvtNormal))
                        {
                            ; // do nothing
                        }
                        else
                        {
                            OUT_DEBUG_3("CAN controller ERROR\r\n");
                            Ar_CAN_processSendingResult(CAN_Tx_ReadyToRepeat);
                        }
                        break;
                    }

                    case OBJ_RELAY:
                    {

                        break;
                    }

                    case OBJ_LED:
                    {

                        break;
                    }

                    case OBJ_BELL:
                    {
                        OUT_DEBUG_7("OBJ_BELL message\r\n");
                        break;
                    }

                    case OBJ_Source:
                    {
//                        ret = Ar_Power_processPowerSourceMsg(mcuPkt);
//                        if (ret < RETURN_NO_ERRORS) {
//                            OUT_DEBUG_1("Ar_Power_processPowerSourceMsg() = %d error.\r\n", ret);
//                        }
                        break;
                    }

                    default:
                        break;
                    }
                    break;
                }

                case OBJ_ETR:
                case OBJ_RZE:
                case OBJ_RAE:
                case OBJ_KEYBOARD:
                {
                    // message from???
                    ParentDevice *pParentFrom = getParentDeviceByUIN(mcumsg.unit.uin, mcumsg.unit.type);

#ifndef NO_CB

                    OUT_DEBUG_2("Message from %s #%d, command %d  \r\n",
                                getUnitTypeByCode(pParentFrom->type), pParentFrom->uin, mcumsg.cmd);


#endif


                    if (CMD_ConnectionRequest == mcumsg.cmd)
                    {
                        ParentDevice *pParent = getParentDeviceByUIN(mcumsg.unit.uin, mcumsg.unit.type);

                        ret = Ar_System_acceptUnitConnectionRequest(&mcumsg.unit, (NULL != pParent));
                        if (ret < RETURN_NO_ERRORS) {
                            OUT_DEBUG_1("Ar_System_acceptUnitConnectionRequest(%s) = %d error.\r\n",
                                        pParent ? "TRUE" : "FALSE", ret);
                        }

                        if (NULL != pParent)
                        {
                            if (OBJ_ETR == pParent->type) {
                                initETR(pParent);
                                Ar_power_updateEtrIndication();

                            }
                        }
                    }
                    else if(CMD_DenyConnectionRequest == mcumsg.cmd) {
                        OUT_DEBUG_2("Duplicate module \r\n");

                        s32 ret = Ar_System_sendEventDuplicateModule(mcumsg.unit.uin, mcumsg.unit.type);
                        if (ret < RETURN_NO_ERRORS) {
                            OUT_DEBUG_1("Ar_System_sendEventDuplicateModule() = %d error\r\n", ret);
                        }
                    }
                    else
                    {
                        ret = Ar_CAN_saveReceivedPkt(mcuPkt);
                        if (ret < RETURN_NO_ERRORS) {
                            OUT_DEBUG_1("Ar_CAN_saveReceivedPkt() = %d error\r\n", ret);
                        }
                    }
                    break;
                }

                default:
                    break;
                }
            }

            // -- go to next MCU packet if any
            mcu_rx_circular_buffer.skip(pktlen);
            counter++;
        }
        else
        {
            mcu_rx_circular_buffer.skip(1);
        }
    }

    return RETURN_NO_ERRORS;
}

//
bool is_AT_command(u8 *data)
{
    return  (((int)data[0] == 'A') || ((int)data[0] == 'a')) &&
            (((int)data[1] == 'T') || ((int)data[1] == 't'));
}

void uart_callback_handler(Enum_SerialPort port, Enum_UARTEventType event,
                                   bool pinLevel, void *customizePara)
{
    switch (event) {
    case EVENT_UART_READY_TO_READ:
    {
        switch (port) {
        case UART_PORT1:
        {
            static u8 _rx_buf[1024] = {0};

            s32 ret = getRxUartData(port, _rx_buf, sizeof(_rx_buf));
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("getRxUartData(port=MAIN) = %d error\r\n", ret);
                return;
            }

            // if AT-command
            if (is_AT_command(_rx_buf)) {
                Ql_UART_Write(VIRTUAL_PORT1, _rx_buf, ret);
                OUT_DEBUG_ATCMD("<<< %s\r\n", _rx_buf);
                break;
            }

            // if NOT AT-command
            ret = parseMainRxUartData(_rx_buf, ret);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("parseMainRxUartData() = %d error\r\n", ret);
            }

            break;
        }

        case UART_PORT2:
        {
            static u8 _rx_buf[1024] = {0};

            s32 ret = getRxUartData(port, _rx_buf, sizeof(_rx_buf));
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("getRxUartData(port=DEBUG) = %d error\r\n", ret);
                return;
            }

            // if AT-command
            if (is_AT_command(_rx_buf)) {
                Ql_UART_Write(VIRTUAL_PORT1, _rx_buf, ret);
                OUT_DEBUG_ATCMD("<<< %s\r\n", _rx_buf);
                break;
            }

            // if NOT AT-command
            ret = parseDebugRxUartData(_rx_buf, ret);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("parseDebugRxUartData() = %d error\r\n", ret);
            }

            break;
        }

        case UART_PORT3:
        {
            static u8 _rx_buf[1024] = {0};

            s32 ret = getRxUartData(port, _rx_buf, sizeof(_rx_buf));
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("getRxUartData(port=AUX) = %d error\r\n", ret);
                return;
            }

            // if NOT AT-command
                ret = parseAuxRxUartData(_rx_buf, ret);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("parseAuxRxUartData() = %d error\r\n", ret);
                }



            break;
        }

        case VIRTUAL_PORT1:
        {
            static u8 _rx_buf[1024] = {0};

            s32 ret = getRxUartData(port, _rx_buf, sizeof(_rx_buf));
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("getRxUartData(port=VIRTUAL) = %d error\r\n", ret);
                return;
            }

            if (!Ql_RIL_IsURCStr((char*)_rx_buf))
                OUT_DEBUG_ATCMD(">>> %s\r\n", _rx_buf);

            break;
        }

        default:
            break;
        }

        break;
    }

    case EVENT_UART_READY_TO_WRITE:
    {
        if (UART_PORT2 == port) {
            _LOCK_OUT_DEBUG_MUTEX
            u8 str[] = "<-EVENT_UARTREADY->\r\n";
            Ql_UART_Write(_DEBUG_UART_PORT, str, sizeof(str));
            CONTINUE_OUT_DEBUG_CYCL;
            _RELEASE_OUT_DEBUG_MUTEX
        }
        break;
    }

    case EVENT_UART_DCD_IND:
        break;
    case EVENT_UART_DTR_IND:
        break;
    case EVENT_UART_RI_IND:
        break;
    case EVENT_UART_FE_IND:
        break;
    }
}


s32 processIncomingPeripheralMessage(u8 *msg)
{

#ifndef NO_CB

    OUT_DEBUG_2("processIncomingPeripheralMessage()\r\n");

#endif

    s32 ret = 0;

    // subdev type evaluation
    switch (msg[MCUPKT_SUBDEV_TYPE]) {
    case OBJ_TAMPER:
    {

        break;
    }

    case OBJ_TOUCHMEMORY_READER:
    {
        ret = process_mcu_touchmemory_key_msg(msg);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("process_mcu_touchmemory_key_msg() = %d error.\r\n", ret);
            return ret;
        }
        break;
    }

    case OBJ_ZONE:
    {
        ret = process_mcu_zone_msg(msg);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("process_mcu_zone_msg() = %d error.\r\n", ret);
            return ret;
        }
        break;
    }

    case OBJ_RELAY:
    {

        break;
    }

    case OBJ_LED:
    {

        break;
    }

    case OBJ_BUTTON:
    {
        ret = process_mcu_button_msg(msg);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("process_mcu_button_msg() = %d error.\r\n", ret);
            return ret;
        }
        break;
    }

    default:
        break;
    }

    return RETURN_NO_ERRORS;
}
