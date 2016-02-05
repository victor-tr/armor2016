#include "configurator.h"
#include "configurator_tx_buffer.h"

#include "common/defines.h"
#include "common/debug_macro.h"

#include "core/system.h"

#include "pult/pult_tx_buffer.h"
#include "pult/pult_rx_buffer.h"

#include "db/db_serv.h"

#include "states/armingstates.h"

#include "service/crc.h"


static ConfiguratorConnectionMode _configurator_connection_mode = CCM_NotConnected;
static u8 transaction_key = 0;
static u8 last_received_pkt_id = 0;


void Ar_Configurator_startWaitingResponse(void)
{ Ar_Timer_startSingle(TMR_ConfiguratorNoResponse_Timer, CONFIGURATION_ROLLBACK_INTERVAL); }

void Ar_Configurator_stopWaitingResponse()
{ Ar_Timer_stop(TMR_ConfiguratorNoResponse_Timer); }

void Ar_Configurator_NoResponse_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_ConfiguratorNoResponse_Timer));
    rollbackConfiguring();
}


s32 Ar_Configurator_processSettingsData(RxConfiguratorPkt *pData, ConfiguratorConnectionMode ccm)
{
    OUT_DEBUG_2("Ar_Configurator_processSettingsData()\r\n");

    switch (pData->marker) {
    case CONFIGURATION_PACKET_MARKER:
    {
        if (CCM_NotConnected != Ar_Configurator_ConnectionMode()
                || PC_CONFIGURATION_BEGIN == pData->dataType)
        {
            if (PC_CONFIGURATION_BEGIN == pData->dataType)  // try enter into configuring mode
            {
                if (CCM_NotConnected == Ar_Configurator_ConnectionMode()) {  // normal switching to configuring
                    if (Ar_Configurator_setConnectionMode(ccm)) {
                        Ar_Configurator_setTransactionKey(pData->sessionKey);
                        Ar_Configurator_setLastRecvPktId(pData->pktIdx - pData->repeatCounter);
                        Ar_Configurator_sendACK(pData->dataType);
                    }
                    else
                    {

                        sendBufferedPacket_configurator_manual(pData->sessionKey,
                                                               pData->pktIdx,
                                                               pData->repeatCounter,
                                                               ccm);
                    }
                    return RETURN_NO_ERRORS;
                } else {                                // configuring already in progress
                    if (Ar_Configurator_transactionKey() == pData->sessionKey) {  // just repeat previous sent pkt
                        Ar_Configurator_stopWaitingResponse();
                        sendBufferedPacket_configurator();
                        return RETURN_NO_ERRORS;
                    } else {                            // error
                        OUT_DEBUG_1("Configuring already in progress.\r\n");
                        return ERR_ALREADY_IN_PROGRESS;
                    }
                }
            }
            else
            {
                return process_received_settings(pData);
            }
        }

        return ERR_DEVICE_NOT_IN_CONFIGURING_STATE;
    }

    case CONFIGURATION_AUTOSYNC_PACKET_MARKER:
    {
        // TODO: process autosync settings data here;

        break;
    }

    default:
        break;
    }

    return RETURN_NO_ERRORS;
}

ConfiguratorConnectionMode Ar_Configurator_ConnectionMode(void)
{ return _configurator_connection_mode; }

bool Ar_Configurator_setConnectionMode(ConfiguratorConnectionMode newMode)
{
    OUT_DEBUG_2("Ar_Configurator_setConnectionMode( newMode=%s )\r\n",
                CCM_NotConnected == newMode ? "READY" : "IN PROGRESS");

    ArmingGroup_tbl *pGroups = getArmingGroupTable(NULL);
    if (!pGroups)
        return FALSE;

    switch (newMode) {
    case CCM_DirectConnection:
    case CCM_RemoteModemConnection:
    case CCM_RemotePultConnection:
        // -- is it possible to switch the device to configuration mode?
        for (u16 i = 0; i < pGroups->size; ++i) {
            if (!Ar_GroupState_isDisarmed(&pGroups->t[i]))
                return FALSE;
        }

        // -- stop any interaction with the pult
        Ar_System_setFlag(SysF_DeviceConfiguring, TRUE);
        pultTxBuffer.clear();
        pultRxBuffer.clear();

        // -- actually switch groups' states
        for (u16 i = 0; i < pGroups->size; ++i) {
            Ar_GroupState_setState(&pGroups->t[i], STATE_CONFIGURING);
        }
        break;

    case CCM_NotConnected:
        for (u16 i = 0; i < pGroups->size; ++i) {
            Ar_GroupState_setState(&pGroups->t[i], STATE_DISARMED);
        }

        Ar_System_setFlag(SysF_DeviceConfiguring, FALSE);
        clearETRsBlocking();    // cancel key requests for all ETRs
        break;
    }

    _configurator_connection_mode = newMode;
    return TRUE;
}


u8 Ar_Configurator_transactionKey(void)
{ return transaction_key; }

void Ar_Configurator_setTransactionKey(u8 key)
{ transaction_key = key; }

u8 Ar_Configurator_lastRecvPktId(void)
{ return last_received_pkt_id; }

void Ar_Configurator_setLastRecvPktId(u8 pkt_id)
{ last_received_pkt_id = pkt_id; }

//
void Ar_Configurator_sendACK(DbObjectCode type)
{
    if (type <= OBJ_MAX) {
        OUT_DEBUG_1("DbObjectCode parameter is too big. No ACK will be sent.\r\n");
        rollbackConfiguring();
        return;
    }

    fillTxBuffer_configurator(type, FALSE, NULL, 0);
    sendBufferedPacket_configurator();
}
