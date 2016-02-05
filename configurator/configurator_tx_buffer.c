#include "ql_uart.h"
#include "ql_stdlib.h"

#include "configurator_tx_buffer.h"
#include "configurator.h"

#include "pult/gprs_udp.h"
#include "pult/pult_tx_buffer.h"

#include "service/crc.h"

#include "core/timers.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "db/db_serv.h"
#include "db/db.h"

#include "codecs/message_codes.h"

#include "service/helper.h"

static u8 main_uart_tx_buffer[TEMP_TX_BUFFER_SIZE];
static u16 main_uart_tx_buffer_size = 0;


void fillTxBuffer_configurator(DbObjectCode file_code, bool bAppend, u8 *data, u16 len)
{
    OUT_DEBUG_2("fillTxBuffer_configurator()\r\n");

    main_uart_tx_buffer[SPS_MARKER] = CONFIGURATION_PACKET_MARKER;
    main_uart_tx_buffer[SPS_PKT_KEY] = Ar_Configurator_transactionKey();
    main_uart_tx_buffer[SPS_APPEND_FLAG] = bAppend;
    main_uart_tx_buffer[SPS_FILECODE] = (u8)file_code;
    main_uart_tx_buffer[SPS_PKT_REPEAT] = 0;

    if (len) {
        main_uart_tx_buffer[SPS_BYTES_QTY_H] = (u8)(len >> 8);
        main_uart_tx_buffer[SPS_BYTES_QTY_L] = (u8)len;
        Ql_memcpy(main_uart_tx_buffer + SPS_PREFIX, data, len);
    } else {
        main_uart_tx_buffer[SPS_BYTES_QTY_H] = 0;
        main_uart_tx_buffer[SPS_BYTES_QTY_L] = 0;
    }

    main_uart_tx_buffer_size = SPS_PREF_N_SUFF + len;
}

s32 sendBufferedPacket_configurator(void)
{
    if (! main_uart_tx_buffer_size)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("sendBufferedPacket_configurator()\r\n");

    main_uart_tx_buffer[SPS_PKT_INDEX] = Ar_Configurator_lastRecvPktId() +
                                                main_uart_tx_buffer[SPS_PKT_REPEAT];
    main_uart_tx_buffer[main_uart_tx_buffer_size - 1] = evaluateConfiguratorCRC(main_uart_tx_buffer);

    s32 ret = 0;

    switch (Ar_Configurator_ConnectionMode()) {
    case CCM_DirectConnection:
        ret = Ql_UART_Write(UART_PORT1, main_uart_tx_buffer, main_uart_tx_buffer_size);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Ql_UART_Write() = %d error\r\n", ret);
        } else if (ret < main_uart_tx_buffer_size) {
            OUT_DEBUG_1("ERROR: Ql_UART_Write() sent %d bytes, but must send %d bytes\r\n",
                        ret, main_uart_tx_buffer_size);
        }
        break;

    case CCM_RemoteModemConnection:
        ret = sendByUDP(main_uart_tx_buffer, main_uart_tx_buffer_size);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("sendByUDP() = %d error\r\n", ret);
        } else if (ret < main_uart_tx_buffer_size) {
            OUT_DEBUG_1("ERROR: sendByUDP() sent %d bytes, but must send %d bytes\r\n",
                        ret, main_uart_tx_buffer_size);
        }
        break;

    case CCM_RemotePultConnection:
        ret = reportToPultComplex(main_uart_tx_buffer, main_uart_tx_buffer_size, PMC_Response_PultSettingsData, PMQ_AuxInfoMsg);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("reportToPultComplex() = %d error\r\n", ret);
        }
        break;

    case CCM_NotConnected:
    default:
        return ERR_DEVICE_NOT_IN_CONFIGURING_STATE;
    }

    main_uart_tx_buffer[SPS_PKT_REPEAT]++;

    const u8 filecode = main_uart_tx_buffer[SPS_FILECODE];
    if (PC_CONFIGURATION_END != filecode)
        Ar_Configurator_startWaitingResponse();

    return RETURN_NO_ERRORS;
}


s32 sendBufferedPacket_configurator_manual(u8 sessionKey, u8 pktIdx, u8 repeatCounter, ConfiguratorConnectionMode ccm)
{

    OUT_DEBUG_2("sendBufferedPacket_configurator_manual()\r\n");

    main_uart_tx_buffer[SPS_MARKER] = CONFIGURATION_PACKET_MARKER;
    main_uart_tx_buffer[SPS_PKT_KEY] = sessionKey;
    main_uart_tx_buffer[SPS_PKT_INDEX] = pktIdx;
    main_uart_tx_buffer[SPS_PKT_REPEAT] = repeatCounter;
    main_uart_tx_buffer[SPS_FILECODE] = PC_CONFIGURATION_DEVICE_ARMED;
    main_uart_tx_buffer[SPS_APPEND_FLAG] = FALSE;
    main_uart_tx_buffer[SPS_BYTES_QTY_H] = 0;
    main_uart_tx_buffer[SPS_BYTES_QTY_L] = 0;

    main_uart_tx_buffer_size = SPS_PREF_N_SUFF;

    main_uart_tx_buffer[main_uart_tx_buffer_size - 1] = evaluateConfiguratorCRC(main_uart_tx_buffer);

    s32 ret = 0;


    switch ( ccm ) {
    case CCM_DirectConnection:
        ret = Ql_UART_Write(UART_PORT1, main_uart_tx_buffer, main_uart_tx_buffer_size);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Ql_UART_Write() = %d error\r\n", ret);
        } else if (ret < main_uart_tx_buffer_size) {
            OUT_DEBUG_1("ERROR: Ql_UART_Write() sent %d bytes, but must send %d bytes\r\n",
                        ret, main_uart_tx_buffer_size);
        }
        break;

    case CCM_RemoteModemConnection:
        ret = sendByUDP(main_uart_tx_buffer, main_uart_tx_buffer_size);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("sendByUDP() = %d error\r\n", ret);
        } else if (ret < main_uart_tx_buffer_size) {
            OUT_DEBUG_1("ERROR: sendByUDP() sent %d bytes, but must send %d bytes\r\n",
                        ret, main_uart_tx_buffer_size);
        }
        break;

    case CCM_RemotePultConnection:
        ret = reportToPultComplex(main_uart_tx_buffer, main_uart_tx_buffer_size, PMC_Response_PultSettingsData, PMQ_AuxInfoMsg);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("reportToPultComplex() = %d error\r\n", ret);
        }
        break;

    case CCM_NotConnected:
    default:
        return ERR_DEVICE_NOT_IN_CONFIGURING_STATE;
    }

    return RETURN_NO_ERRORS;
}


void clearTxBuffer_configurator( void )
{
    Ql_memset(main_uart_tx_buffer, 0, main_uart_tx_buffer_size);
    main_uart_tx_buffer_size = 0;
}
