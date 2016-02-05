#include "ql_stdlib.h"
#include "ql_power.h"
#include "ql_wtd.h"

#include "atcommand.h"
#include "canbus.h"
#include "power.h"

#include "core/system.h"
#include "core/simcard.h"
#include "core/mcu_tx_buffer.h"
#include "core/timers.h"
#include "core/power.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "service/crc.h"
#include "service/humanize.h"

#include "pult/pult_tx_buffer.h"
#include "pult/gprs_udp.h"

#include "states/armingstates.h"

#include "db/lexicon.h"
#include "db/db.h"
#include "db/db_serv.h"


#define SOFTWARE_TIMERS_INTERVAL  100   // ms

#define ERROR_BUFFER_TIMERS_INTERVAL  30000   // ms refresh error message
#define COUNT_OF_ERROR_BUFFER_TIMERS_INTERVALS  2  // raz count cicl  (refresh error message)

#define DECLARE_PRESCALER         static u16 prescaler = 1; ++prescaler;

// value is in ms
#define INTERVAL_MS(value) \
    (!(prescaler % ((value < SOFTWARE_TIMERS_INTERVAL) ? 1 : (value / SOFTWARE_TIMERS_INTERVAL))))



typedef enum _StartupInitState {
    SIS_Uninitialized                   = 1,

    SIS_SwitchSimCard                   = 2,
    SIS_WaitingForActiveSimcard         = 3,
    SIS_InitializeConnections           = 4,
    SIS_DiscoverParentUnits             = 5,
    SIS_WaitingForMcuRestart            = 6,
    SIS_InitializedOk                   = 7,

    SIS_InitializationError             = 8,

    SIS_CommonWaiting                   = 10,
    SIS_AboutToPowerOff                 = 20
} StartupInitState;


static u8 _system_flags = 0;

static u32 size_all_files = 0;

static StartupInitState _INIT_STATE_ = SIS_Uninitialized;
static u32 init_hangup_counter = 0;

static u32 system_error_buffer_counter_no_cicles = 0;
static u32 system_error_buffer_counter = 0;
static u8 _system_error_buffer[8]={0};
static bool bRunSystemErrorBufferCicl=TRUE;
static u8 system_error_buffer_delay_counter=0;

static bool bFOTA_process = FALSE;

static bool bAudio = FALSE;

static float last_voltage=0;

static u16 PSS_Freze_Counter = 0;

static s32 WatchDogID =0;

void Ar_System_setFlag(SystemFlag flag, bool state)
{ _system_flags = state ? (_system_flags | flag) : (_system_flags & ~flag); }

bool Ar_System_checkFlag(SystemFlag flag)
{ return (_system_flags & flag) == flag; }

void Ar_System_resetFlags(void)
{ _system_flags = 0; }


u32 Ar_System_readSizeAllFiles(){
    return size_all_files;
}

void Ar_System_writeSizeAllFiles(u32 add){
   size_all_files = add;
}

static void System_setStartupInitState(StartupInitState state)
{
    _INIT_STATE_ = state;
    init_hangup_counter = 0;
}

void Ar_System_StartDeviceInitialization(void)
{
    static bool runOnce_Flag = TRUE;

    OUT_DEBUG_2("Ar_System_StartDeviceInitialization()\r\n");

    if (runOnce_Flag) {
        runOnce_Flag = FALSE;
        Ar_Lexicon_register();
        Ar_CAN_registerMessageHandler(&processIncomingPeripheralMessage);
        Ar_SIM_registerCallbacks();
        Ar_Power_registerADC();
        Ar_Timer_registerTimers();   // !!! register all timers before any of timers will be started
    }
    else {
        Ar_Power_enableBatterySampling(FALSE);
    }

    Ar_System_resetFlags();
    System_setStartupInitState(SIS_Uninitialized);

    Ar_Timer_startSingle(TMR_StartupInit_Timer, 1);
}

static void System_finishInitSuccessfully(void)
{
    //_bCanHandleMcuMsgs = TRUE;    actually enabled at another place
    init_hangup_counter = 0;
    Ar_System_setFlag(SysF_DeviceInitialized, TRUE);
    Ar_Power_enableBatterySampling(TRUE);

    OUT_DEBUG_3("Successfully initialized. Ready to work.\r\n");

    // -- notify the pult that the device was powered on
    s32 ret = Ar_System_sendEventPowerON();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("Ar_System_sendEventPowerON() = %d error\r\n", ret);
    }

    // --
    ret = Ar_System_sendEventPowerONFromBattery();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("Ar_System_sendEventPowerON() = %d error\r\n", ret);
    }



}

// WARNING: expected 2 items in each list
static bool System_isMinimalConfigOk(void)
{
    SimSettings *ss_array = getSimSettingsArray(NULL);

    bool sim1_configured = ss_array[SIM_1].can_be_used &&
                            ss_array[SIM_1].prefer_gprs &&
                            (isIPAddressValid(&ss_array[SIM_1].udp_ip_list._IPs_m[0]) ||
                            isIPAddressValid(&ss_array[SIM_1].udp_ip_list._IPs_m[1]));

    sim1_configured = sim1_configured ||
                        (
                            ss_array[SIM_1].can_be_used &&
                            !ss_array[SIM_1].prefer_gprs &&
                            (ss_array[SIM_1].phone_list._phones_m[0].number[0] ||
                            ss_array[SIM_1].phone_list._phones_m[1].number[0])
                        );


    bool sim2_configured = ss_array[SIM_2].can_be_used &&
                            ss_array[SIM_2].prefer_gprs &&
                            (isIPAddressValid(&ss_array[SIM_2].udp_ip_list._IPs_m[0]) ||
                             isIPAddressValid(&ss_array[SIM_2].udp_ip_list._IPs_m[1]));

    sim2_configured = sim2_configured ||
                        (
                            ss_array[SIM_2].can_be_used &&
                            !ss_array[SIM_2].prefer_gprs &&
                            (ss_array[SIM_2].phone_list._phones_m[0].number[0] ||
                            ss_array[SIM_2].phone_list._phones_m[1].number[0])
                        );

    return sim1_configured || sim2_configured;
}

static bool System_isDeviceConfigured(void)
{
    bool configured = System_isMinimalConfigOk();

    OUT_DEBUG_2("System_isDeviceConfigured(): %s\r\n", configured ? "YES" : "NO");

    if (!configured) {
        OUT_DEBUG_1("The device is not configured yet\r\n");
        return FALSE;
    }

    return TRUE;
}

void Ar_System_blockHandlingMCUMessages(bool block)
{ Ar_System_setFlag(SysF_CanHandleMcuMessages, !block); }

bool Ar_System_isSimSlotSwitchingAllowed(void)
{
    return SIS_WaitingForActiveSimcard == _INIT_STATE_ ||
            SIS_InitializedOk == _INIT_STATE_;
}


static const char *System_getInitStateByCode(u8 code)
{
    switch (code) {
    CASE_RETURN_NAME(SIS_Uninitialized);
    CASE_RETURN_NAME(SIS_SwitchSimCard            );
    CASE_RETURN_NAME(SIS_WaitingForActiveSimcard  );
    CASE_RETURN_NAME(SIS_InitializeConnections    );
    CASE_RETURN_NAME(SIS_DiscoverParentUnits      );
    CASE_RETURN_NAME(SIS_WaitingForMcuRestart     );
    CASE_RETURN_NAME(SIS_InitializedOk            );
    CASE_RETURN_NAME(SIS_InitializationError);
    CASE_RETURN_NAME(SIS_CommonWaiting);
    CASE_RETURN_NAME(SIS_AboutToPowerOff);
    DEFAULT_RETURN_CODE(code);
    }
}


static s32 System_startupInitialization_Step(void)
{
    OUT_DEBUG_2("System_startupInitialization_Step(<%s>)\r\n", System_getInitStateByCode(_INIT_STATE_));

    /* restart if any step hangup */
    if (init_hangup_counter > 30) {
        OUT_DEBUG_1("Device initialization stopped by timeout\r\n");
        Ar_System_requestHardRestartMe();
        return ERR_DEVICE_INITIALIZATION_HANGUP;
    }

    /* initialization */
    s32 ret = 0;

    switch (_INIT_STATE_) {
    case SIS_Uninitialized:
    {
        // -- init working DB

        ret = reloadDBinRAM();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("reloadDBinRAM() = %d error\r\n", ret);
            return ret;
        }

        OUT_DEBUG_7("Total  database size  = %d bytes\r\n", Ar_System_readSizeAllFiles());

        ret = Ar_System_indicateInitializationProgress(TRUE);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Ar_System_indicateInitializationProgress() = %d error\r\n", ret);
            return ret;
        }

        System_setStartupInitState(SIS_SwitchSimCard);
        break;
    }

    case SIS_SwitchSimCard:
    {
        if (System_isDeviceConfigured()) {
            // --
            if (Ar_SIM_canSlotBeUsed(SIM_1)) {
                Ar_SIM_ActivateSlot(SIM_1);
            } else if (Ar_SIM_canSlotBeUsed(SIM_2)) {
                Ar_SIM_ActivateSlot(SIM_2);
            } else {
                OUT_DEBUG_1("NO ALLOWED SIM CARDS\r\n");
                return ERR_SIM_ACTIVATION_FAILED;
            }

            System_setStartupInitState(SIS_WaitingForActiveSimcard);
        }
        else {
            return ERR_DEVICE_IS_NOT_CONFIGURED;
        }
        break;
    }

    case SIS_WaitingForActiveSimcard:
    {
        bool failed = FALSE;

        if (init_hangup_counter > 15) {
            init_hangup_counter = 0;
            failed = TRUE;
        }

        if (Ar_SIM_isSlotActivationFailed()) {
            Ar_SIM_blockSlot(Ar_SIM_newSlot());
            init_hangup_counter = 0;
            failed = TRUE;
        }

        if (Ar_SIM_isSimCardReady())
        {
            System_setStartupInitState(SIS_InitializeConnections);
        }
        else if (failed)
        {
            if (SIM_2 == Ar_SIM_newSlot() && Ar_SIM_canSlotBeUsed(SIM_1)) {
                Ar_SIM_ActivateSlot(SIM_1);
            } else if (SIM_1 == Ar_SIM_newSlot() && Ar_SIM_canSlotBeUsed(SIM_2)) {
                Ar_SIM_ActivateSlot(SIM_2);
            } else {
                OUT_DEBUG_1("NO AVAILABLE SIM CARDS. Initialization stopped.\r\n");
                return ERR_SIM_ACTIVATION_FAILED;
            }
        }

        break;
    }

    case SIS_InitializeConnections:
    {
        // -- init all pult channels
        ret = preinitPultConnections();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("preinitPultConnections() = %d error.\r\n", ret);
            return ret;
        }
        System_setStartupInitState(SIS_DiscoverParentUnits);
        break;
    }

    case SIS_DiscoverParentUnits:
    {
        Ar_System_setFlag(SysF_CanHandleMcuMessages, TRUE);

        ret = Ar_System_requestMcuRestart();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Ar_System_requestMcuRestart() = %d error\r\n", ret);
            return ret;
        }
        System_setStartupInitState(SIS_WaitingForMcuRestart);
        break;
    }

    case SIS_WaitingForMcuRestart:  // STARTUP_INIT_TIMER_INTERVAL delay => MCU have time to restart
    {
        System_setStartupInitState(SIS_InitializedOk);
        break;
    }

    case SIS_InitializedOk:
    {
        System_finishInitSuccessfully();
        Ar_System_indicateInitializationProgress(FALSE);

        /* start software timers processing */
        ret = Ar_Timer_startContinuous(TMR_SoftwareTimersSet, SOFTWARE_TIMERS_INTERVAL);
        OUT_DEBUG_3("Ar_Timer_startContinuous(TMR_SoftwareTimersSet): %s\r\n", QL_RET_OK == ret ? "OK" : "FAIL");

        //Send one time AT commsnd
        char cmd[100] = {0};
        Ql_sprintf(cmd, "AT+QSFR=0");
        s32 retAT = Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), NULL, NULL, 0);
        if (retAT != RIL_AT_SUCCESS){
            OUT_DEBUG_1("AT+QSFR = %d error.\r\n", retAT);
        }



        return RETURN_NO_ERRORS;
    }

    case SIS_InitializationError:
    {
        return ERR_DEVICE_INITIALIZATION_ERROR;
    }

    case SIS_CommonWaiting:
        OUT_DEBUG_3("Wait for the end of the init step...\r\n");
        break;

    case SIS_AboutToPowerOff:
    {
        // TODO: wait for pultTxBuffer is empty then start Ql_PowerOff function here ??
//        if (pultTxBuffer.isEmpty()) {
//            Ar_System_setFlag(SysF_DeviceInitialized|SysF_CanHandleMcuMessages, FALSE);
//            Ql_PowerDown(1);
//            return RETURN_NO_ERRORS;
//        }
        OUT_DEBUG_3("System_startupInitialization_Step(): SIS_AboutToPowerOff -> NORMAL POWER DOWN must ????\r\n");

        return Ar_Timer_startSingle(TMR_StartupInit_Timer, STARTUP_INIT_TIMER_INTERVAL);
    }

    default:
        OUT_DEBUG_3("System_startupInitialization_Step(): Unhandled _INIT_STATE_ %d\r\n", _INIT_STATE_);
        break;
    }

    ++init_hangup_counter;
    return Ar_Timer_startSingle(TMR_StartupInit_Timer, STARTUP_INIT_TIMER_INTERVAL);
}

void System_startupInit_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_StartupInit_Timer));
    s32 ret = System_startupInitialization_Step();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("System_startupInitialization_Step() = %d error. The device hasn't passed the initialization process.\r\n", ret);

        if (ERR_CANNOT_CREATE_RAM_DATABASE == ret
                || ERR_CANNOT_RESTORE_DB_BACKUP == ret)
        {
            ;            // ??? stop executing if DB failed
            // TODO: implement some error indication
        }
        else
        {
            ;
        }
    }
}


//
s32 Ar_System_sendEventPowerON(void)
{
    OUT_DEBUG_2("Ar_System_sendEventPowerON()\r\n");

    u16 AutoSyncID = getSystemInfo(NULL)->settings_signature_autosync;
    s32 ret = reportToPult(0,AutoSyncID,PMC_SystemPowerOn,PMQ_RestoralOrSetArmed);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

//
s32 Ar_System_sendEventDuplicateModule(u16 uin, DbObjectCode type)
{
    OUT_DEBUG_2("Ar_System_sendEventDuplicateModule()\r\n");

    u8 data[2] = {type,
                  uin};
    s32 ret = reportToPultComplex(data, sizeof(data),PMC_SystemDuplicateModule,PMQ_RestoralOrSetArmed);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}





s32 Ar_System_sendEventPowerONFromBattery(void)
{
    const bool b_AC_ok = Ar_Power_startFlag();
    OUT_DEBUG_2("Ar_System_sendEventPowerONFromBattery(). b_AC_ok = %d\r\n", b_AC_ok);




    if(!b_AC_ok)
    {
        s32 ret = reportToPult(0, 0, PMC_AC_Loss, PMQ_NewMsgOrSetDisarmed);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
            return ret;
        }
    }

    return RETURN_NO_ERRORS;
}

// *********************************************************************************************************
// -- MCU
void Ar_System_markUnitAsConnected(UnitIdent *unit)
{
    // wait for registering the masterboard UIN
    if (0 == getCommonSettings(NULL)->p_masterboard->uin)
        return;

    ParentDeviceConnectionState *pState = NULL;

    if (OBJ_MASTERBOARD != unit->type)
    {
        pState = &getCommonSettings(NULL)->p_masterboard->connection_state;

        if (UnitDisconnectedReported == pState->value)
            pState->value = UnitConnected;
        else if (UnitAfterPowerOn == pState->value)
            pState->value = UnitConnectedReported;

        pState->counter = 0;
    }

    ParentDevice *pParent = getParentDeviceByUIN(unit->uin, unit->type);

    if (pParent) {
        pState = &pParent->connection_state;

        if (UnitDisconnectedReported == pState->value)
            pState->value = UnitConnected;
        else if (UnitAfterPowerOn == pState->value)
            pState->value = UnitConnectedReported;

        pState->counter = 0;
    }
    else {
        OUT_DEBUG_1("Ar_System_markUnitAsConnected(): Unit <%s #%d> not found in DB\r\n",
                    getUnitTypeByCode(unit->type), unit->uin)
    }
}

s32 Ar_System_requestHardRestartMe(void)
{
    OUT_DEBUG_2("Ar_System_requestHardRestartMe()\r\n");

    // OBJ_Output used here for restarting GSM-module
    CREATE_UNIT_IDENT(unit,
                      OBJ_MASTERBOARD, getCommonSettings(NULL)->p_masterboard->uin,
                      OBJ_Output, 0x01);

    u8 data[] = {Pin_Input, 0x30, 0x30, 0x32, 0x00};

    return sendToMCU(&unit, CMD_SetValue, data, sizeof(data));
}

s32 Ar_System_requestMcuRestart(void)
{
    OUT_DEBUG_2("Ar_System_requestMcuRestart()\r\n");

    MasterBoard *pMasterboard = &getCommonSettings(NULL)->p_masterboard->d.masterboard;
    pMasterboard->bMcuApproved = FALSE;
    pMasterboard->bMcuConnectionRequested = FALSE;

    // -- restart AuxMCU
    McuCommandRequest r2 = {{0}};
    r2.unit.type = OBJ_MASTERBOARD;
    r2.unit.uin = getCommonSettings(NULL)->p_masterboard->uin;
    r2.unit.stype = OBJ_AUX_MCU;
    r2.unit.suin = 0x00;
    r2.cmd = CMD_Restart;

    return sendToMCU_2(&r2);
}


s32 Ar_System_WDT(u8 sec)
{
    OUT_DEBUG_2("------------------------------------\r\n", sec );
    OUT_DEBUG_2("Ar_System_WDT(interval=%d * 100 ms)\r\n", sec );

    CREATE_UNIT_IDENT(unit,
                      OBJ_MASTERBOARD, getCommonSettings(NULL)->p_masterboard->uin,
                      OBJ_WatchDog, 0x01);

    u8 data[6] = {Pin_Input, 0x30, 0x30, 0x31, 0x00, sec};

    s32 retReset = sendToMCU(&unit, CMD_SetValue, data, sizeof(data));
    return retReset;
}

s32 Ar_System_aboutToPowerOff(u8 reason)
{
    OUT_DEBUG_2("Ar_System_aboutToPowerOff(reason=%d)\r\n", reason);

    // TODO: set 'aboutToPowerOff_flag' to disable ETRs' initialization on their requests

    // -- restart all ETRs
//    McuCommandRequest r2 = {{0}};
//    r2.unit.type = OBJ_ETR;
//    r2.unit.uin = 0;
//    r2.unit.stype = 0;
//    r2.unit.suin = 0x00;
//    r2.cmd = CMD_Restart;

    s32 ret = 0;
//    ret = sendToMCU_2(&r2);
//    System_setStartupInitState(SIS_AboutToPowerOff);
//    Ar_Timer_startSingle(TMR_StartupInit_Timer, 1);
    return ret;
}

s32 Ar_System_enableBellAutoreport(UnitIdent *unit, bool enable)
{
    OUT_DEBUG_2("Ar_System_enableBellAutoreport(%s)\r\n", enable ? "Yes" : "No");

    if (OBJ_BELL != unit->stype)
        return ERR_BAD_UNIT_TYPE;

    u8 cmd_byte = enable ? 0x33 : 0x34; // 003 - autoreport; 004 - requested report
    u8 data[] = {Pin_Control, 0x30, 0x30, cmd_byte, 0x00};
    return sendToMCU(unit, CMD_SetValue, data, sizeof(data));
}

s32 Ar_System_acceptUnitConnectionRequest(UnitIdent *unit, bool accept)
{
    OUT_DEBUG_2("Ar_System_acceptUnitConnectionRequest(%s #%d, accept=%s)\r\n",
                getUnitTypeByCode(unit->type), unit->uin, accept ? "TRUE" : "FALSE");

#ifdef NO_SEND_HEARTBEAT
    u8 data[1] = {0}; // do not send heartbeat pkt to GSM module
#else
    u8 data[1] = {100}; // heartbeat timeout = 600 * 100ms = 10 sec
#endif

    McuCommandCode accept_code = accept ? CMD_AcceptConnectionRequest : CMD_DenyConnectionRequest;
    return sendToMCU(unit, accept_code, data, 1);
}

s32 Ar_System_indicateInitializationProgress(bool isActive)
{
    OUT_DEBUG_2("Ar_System_indicateInitializationProgress(isActive=%s)\r\n", isActive ? "TRUE" : "FALSE");

    Led *pLed = getLedByParent(getCommonSettings(NULL)->p_masterboard->uin, OBJ_MASTERBOARD, 0x02);
    if (!pLed) return ERR_DB_RECORD_NOT_FOUND;

    PerformerUnitState state = 0;
    BehaviorPreset behavior = {0};

    if (isActive) {
        state = UnitStateMultivibrator;
        behavior.pulse_len = 2;   // infinite meandr
        behavior.pause_len = 3;
    } else {
        state = UnitStateOff;
        Ar_System_errorBeepEtrBuzzer(2, 1, 3);  // indicate initialization finished successfully
    }

    return setLedUnitState(pLed, state, &behavior);
}

// *********************************************************************************************************


//
void Ar_System_setZoneAlarmedFlag(Zone *pZone, bool enabled)
{
    OUT_DEBUG_2("Ar_System_setZoneAlarmedFlag(hum_id=%d, %s)\r\n",
                pZone->humanized_id, enabled ? "TRUE" : "FALSE");

    if (!pZone)
        return;

    if (enabled) {
        pZone->zoneDamage_counter += 1;
    } else {
        pZone->zoneDamage_counter = 0;
        //
               PultMessage msg;
               msg.identifier = pZone->humanized_id;
               msg.group_id = pZone->group_id;
               msg.msg_code = PMC_ProtectionLoopTrouble;
               msg.msg_qualifier = PMQ_RestoralOrSetArmed;
               s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
               if (ret < RETURN_NO_ERRORS) {
                   OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
                   return;
               }

    }

OUT_DEBUG_8("Zone: %d, Flag: %d, Counter: %d\r\n",
pZone->humanized_id, enabled, pZone->zoneDamage_counter);
}

//
static void System_processAlarmedZones(void)
{
//    OUT_DEBUG_2("System_processAlarmedZones()\r\n");

    Zone_tbl *zt = getZoneTable(NULL);
    Zone *pZone = 0;

    for (u8 zone_pos = 0; zone_pos < zt->size; ++zone_pos)
    {
        pZone = &zt->t[zone_pos];

        if (!pZone->damage_notification_enabled)
            continue;

        if (pZone->zoneDamage_counter > 0 &&
                pZone->zoneDamage_counter < ZONE_DAMAGED_THRESHOLD)
        {
            if (++pZone->zoneDamage_counter == ZONE_DAMAGED_THRESHOLD) {
                OUT_DEBUG_3("Zone %d is DAMAGED.\r\n", pZone->humanized_id);
                if (ZONE_PANIC != pZone->zone_type && ZONE_ENTRY_DELAY != pZone->zone_type)
                {
                       PultMessage msg;
                       msg.identifier = pZone->humanized_id;
                       msg.group_id = pZone->group_id;
                       msg.msg_code = PMC_ProtectionLoopTrouble;
                       msg.msg_qualifier = PMQ_NewMsgOrSetDisarmed;
                       s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
                       if (ret < RETURN_NO_ERRORS) {
                           OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
                           return;
                       }
                }
            }
        }
    }
}

static void System_processDelayedZones(void)
{
//    OUT_DEBUG_2("System_processDelayedZones()\r\n");

    CommonSettings *cs = getCommonSettings(NULL);
    ArmingGroup_tbl *agt = getArmingGroupTable(NULL);
    ArmingGroup *pGroup = 0;

    for (u16 group_pos = 0; group_pos < agt->size; ++group_pos)
    {
        pGroup = &agt->t[group_pos];
        /* when switch to arming */
        if (pGroup->arming_zoneDelay_counter > 0 &&
                pGroup->arming_zoneDelay_counter < cs->arming_delay_sec)
        {
            OUT_DEBUG_7("System_processDelayedZones() arming_zoneDelay_counter=%d  arming_delay_sec=%d\r\n",
                        pGroup->arming_zoneDelay_counter,cs->arming_delay_sec);

            StopSystemErrorBufferCicl();
            if(pGroup->arming_zoneDelay_counter==1)
            {
                Ar_System_startEtrArmedLED(pGroup, 1, 1, 4);
                Ar_System_startEtrBuzzer(pGroup, 1, 1, 4, FALSE);
            }
            if(pGroup->arming_zoneDelay_counter==2)
            {
                Ar_System_startEtrArmedLED(pGroup, 6, 4, 0);
                Ar_System_startEtrBuzzer(pGroup, 6, 4, 0, TRUE);
            }

            if (++pGroup->arming_zoneDelay_counter == cs->arming_delay_sec) {

                Ar_System_stopPrevETRBuzzer(pGroup);
                // test_entry
                //Ar_System_setPrevETRLed(pGroup,UnitStateOn);

                StartSystemErrorBufferCicl();

                OUT_DEBUG_3("arming_zoneDelay_counter TIMEOUT\r\n");
                pGroup->arming_zoneDelay_counter = 0;
                if(TRUE != Ar_System_isZonesInGroupNormal(pGroup))
                {
                    Zone_tbl *zones_table = getZoneTable(NULL);

                    for (u16 pos = 0; pos < zones_table->size; ++pos) {
                        if (pGroup->id ==zones_table->t[pos].group_id){
                            if (TRUE == zones_table->t[pos].enabled)
                            {
                                if (ZONE_ENTRY_DELAY == zones_table->t[pos].zone_type || ZONE_WALK_THROUGH == zones_table->t[pos].zone_type )
                                {
                                    // -- send alarm: the door opened
                                    s32 ret = reportToPult(zones_table->t[pos].humanized_id,
                                                           pGroup->id,
                                                           PMC_Burglary,
                                                           PMQ_NewMsgOrSetDisarmed);
                                    Ar_System_setBellState(UnitStateMultivibrator);

                                }
                            }
                        }
                    }
                }

            }
        }
        /* when switch to disarming */
        else if (pGroup->disarming_zoneDelay_counter > 0 &&
                       pGroup->disarming_zoneDelay_counter < cs->entry_delay_sec)
        {
            StopSystemErrorBufferCicl();
            OUT_DEBUG_7("System_processDelayedZones() disarming_zoneDelay_counter=%d  entry_delay_sec=%d\r\n",
                        pGroup->disarming_zoneDelay_counter,cs->entry_delay_sec);

            if (++pGroup->disarming_zoneDelay_counter == cs->entry_delay_sec)
            {
                Ar_System_setPrevETRLed(pGroup, UnitStateOn);
                Ar_System_stopPrevETRBuzzer(pGroup);

                StartSystemErrorBufferCicl();

                pGroup->disarming_zoneDelay_counter = 0;
                OUT_DEBUG_3("arming_zoneDelay_counter TIMEOUT\r\n");
                Ar_System_setBellState(UnitStateMultivibrator);
                // -- send alarm: the door opened
                s32 ret = reportToPult(pGroup->entry_delay_zone
                                            ? pGroup->entry_delay_zone->id
                                            : DB_VALUE_NULL,
                                       pGroup->id,
                                       PMC_Burglary,
                                       PMQ_NewMsgOrSetDisarmed);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
                    return;
                }
            }
        }
    }
}


static void System_processParentUnitsConnections(void)
{
    /*
     *  should be disabled  because initialization is longer than the threshold delay =>
     *  hence a lost connection will be mistakenly detected
     */
    if (!Ar_System_checkFlag(SysF_DeviceInitialized))
        return;

    //OUT_DEBUG_2("System_processParentUnitsConnections()\r\n");

    ParentDevice_tbl *pParents = getParentDevicesTable(NULL);

    u16 *pCounter = NULL;
    ParentDeviceConnectionStateValue *pState = NULL;
    ParentDevice *pParent = NULL;

    for (u8 pos = 0; pos < pParents->size; ++pos)
    {
        pParent = &pParents->t[pos];

        /*
         *  the masterboard doesn't have any link service
         *  so it should be ignored in the polling
         */
        if (OBJ_MASTERBOARD == pParent->type)
            continue;

        pCounter = &pParent->connection_state.counter;
        pState = &pParent->connection_state.value;

//        OUT_DEBUG_8("--PARENT^ %s, #%d, state-%d, counter-%d\r\n",
//                    getUnitTypeByCode(pParents->t[pos].type),
//                    pParents->t[pos].uin, *pState, *pCounter);

        switch (*pState) {
        case UnitAfterPowerOn:
            if (*pCounter >= PARENT_DEVICE_SILENCE_AFTER_POWERON_THRESHOLD) {
                reportToPult(pParent->uin, pParent->type, PMC_ExpansionModuleFailure, PMQ_NewMsgOrSetDisarmed);
                *pCounter = 0;
                *pState = UnitDisconnectedReported;
            } else {
                ++*pCounter;
            }
            break;

        case UnitConnected:
            reportToPult(pParent->uin, pParent->type, PMC_ExpansionModuleFailure, PMQ_RestoralOrSetArmed);
            *pState = UnitConnectedReported;
            break;

        case UnitConnectedReported:
            if (*pCounter >= PARENT_DEVICE_SILENCE_THRESHOLD) {
                reportToPult(pParent->uin, pParent->type, PMC_ExpansionModuleFailure, PMQ_NewMsgOrSetDisarmed);
                *pCounter = 0;
                *pState = UnitDisconnectedReported;
            } else {
                ++*pCounter;
            }
            break;

        default:
            break;
        }
    }
}

static void System_process_debug_output(void)
{
    _LOCK_OUT_DEBUG_MUTEX
    if (DBS_ProcessLater == textBuf.status)
    {
        u8 str[] = "<-System_process_debug_output->\r\n";
        Ql_UART_Write(_DEBUG_UART_PORT, str, sizeof(str));
        CONTINUE_OUT_DEBUG_CYCL;
    }
    _RELEASE_OUT_DEBUG_MUTEX
}

// stop cicl error message and start delay
void StopSystemErrorBufferCicl()
{
    system_error_buffer_counter=0;
    bRunSystemErrorBufferCicl=FALSE;
}

// run cicl error message  and start delay
void StartSystemErrorBufferCicl()
{
    system_error_buffer_counter=0;
    bRunSystemErrorBufferCicl=TRUE;
}

// set a flag the presence of one error
void setFlagSystemErrorCodeInBuffer(SystemErrorCode  code)
{
        _system_error_buffer[code]=1;
}

// reset a flag the presence of one error
void resetFlagSystemErrorCodeInBuffer(SystemErrorCode  code)
{
        _system_error_buffer[code]=0;
}

// Search the next error in the buffer
static SystemErrorCode NextFlagSystemErrorCodeInBuffer()
{
     SystemErrorCode sec;

     for (u8 i = 0; i <= 7; ++i)
     {
        if (++system_error_buffer_counter>7) system_error_buffer_counter=0;

        if(_system_error_buffer[system_error_buffer_counter]==1)
        {
            sec=system_error_buffer_counter;
            break;
        }
        else
        {
            sec=SEC_ClearLastError;
            continue;
        }
     }
    return sec;

}


//
static void System_processErrorBufferNotify(void)
{

        SystemErrorCode sec=NextFlagSystemErrorCodeInBuffer();
        if (SEC_ClearLastError!=sec)
        {
        BehaviorPreset behavior = {0};
        behavior.pulse_len = 2;
        behavior.pause_len = 1;
        behavior.pulses_in_batch = sec;

        Led led;            // system (trouble) LED
        led.ptype = OBJ_ETR;
        led.puin = 0x00;
        led.suin = 0x04;

        EtrBuzzer buzzer;
        buzzer.puin = 0x00;

        s32 ret ;
        ret = setEtrBuzzerUnitState(&buzzer, UnitStateMultivibrator, &behavior);
        ret = setLedUnitState(&led, UnitStateMultivibrator, &behavior);

        }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++
void Ar_System_processSoftwareTimers(void *data)
{

    DECLARE_PRESCALER;


    if (INTERVAL_MS(10000)) // one minute counter
    {
//        OUT_DEBUG_2("######################## AT+QWDTMF=7,0,\"1401370077000017,220,140\" ############################\r\n");

//        char cmd[100] = {0};
//                Ql_sprintf(cmd, "AT+QWDTMF=7,0,\"1401370077000017,220,140\"");

//        s32 retSend = Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), NULL, NULL, 0);



    }


    if (INTERVAL_MS(100))
    {
        System_process_debug_output();
    }

    if (INTERVAL_MS(1000))
    {
        System_processAlarmedZones();
        System_processDelayedZones();
        Ar_CAN_checkBusActivity();
        System_processParentUnitsConnections();

    }

    if (INTERVAL_MS(10000))
    {
        // WDT send
        Ar_System_WDT(200);
        OUT_DEBUG_2("--------------------------\r\n");
        OUT_DEBUG_2("Prepared to send to pult (pultTxBuffer.size) -> %d \r\n", pultTxBuffer.size());
        OUT_DEBUG_2("PSS_Freeze_Counter = %d \r\n", Ar_System_getPSSFreezeCounter());
        OUT_DEBUG_2("--------------------------\r\n");
    }

    if (INTERVAL_MS(60000)) // one minute counter
    {
        u8 data[1] = {1};
        s32 rettt = reportToPultComplex(data, sizeof(data), PMC_ConnectionTest, PMQ_AuxInfoMsg);

        if( 10 < ++PSS_Freze_Counter){
            PSS_Freze_Counter = 0;
            OUT_DEBUG_2("PSS_Freze_Counter > 60()\r\n");
            Ar_Timer_startSingle(TMR_PultSession_Timer, 1000);
        }

    }

    if (INTERVAL_MS(ERROR_BUFFER_TIMERS_INTERVAL))
    {

        if(bRunSystemErrorBufferCicl)
        {
            System_processErrorBufferNotify();
        }
        else
        {
            if(++system_error_buffer_counter>(COUNT_OF_ERROR_BUFFER_TIMERS_INTERVALS))
            {
                StartSystemErrorBufferCicl();
            }
        }
    }
}


void Ar_System_errorBeepEtrBuzzer(u8 pulse_len, u8 pause_len, u8 nPulses)
{
    OUT_DEBUG_2("Ar_System_errorBeepEtrBuzzer()\r\n");

    BehaviorPreset behavior = {0};
    behavior.pulse_len = pulse_len;
    behavior.pause_len = pause_len;
    behavior.pulses_in_batch = nPulses;

    EtrBuzzer buzzer;
    buzzer.puin = 0x00;

    s32 ret = setEtrBuzzerUnitState(&buzzer, UnitStateMultivibrator, &behavior);
    if (ret < RETURN_NO_ERRORS)
        OUT_DEBUG_1("setEtrBuzzerUnitState() = %d error\r\n", ret);
}


void Ar_System_BeepEtrBuzzerAndSystemLED( ParentDevice *pParent, u8 pulse_len, u8 pause_len, u8 nPulses)
{
    OUT_DEBUG_1("Ar_System_BeepEtrBuzzerAndSystemLED() \r\n");
    if(!pParent)
        return;
    if(OBJ_ETR==pParent->type){
        BehaviorPreset behavior = {0};
        behavior.pulse_len = pulse_len;
        behavior.pause_len = pause_len;
        behavior.pulses_in_batch = nPulses;

        Led led;            // system (trouble) LED
        led.ptype = OBJ_ETR;
        led.puin = pParent->uin;
        led.suin = 0x01;


        EtrBuzzer buzzer;
        buzzer.puin = pParent->uin;
        s32 ret = 0;

        if(pParent->d.etr.buzzer.counter==0)
        {
            setEtrBuzzerUnitStateALL(&buzzer, UnitStateMultivibrator, &behavior);
            setLedUnitStateALL(&led, UnitStateMultivibrator, &behavior);

        }



    }
}



//========================

void Ar_System_startEtrArmedLED(ArmingGroup *pAG, u8 pulse_len, u8 pause_len, u8 nPulses)
{
    OUT_DEBUG_1("Ar_System_startEtrArmedLED() \r\n");

    if(OBJ_ETR==pAG->change_status_ETR->type){
        BehaviorPreset behavior = {0};
        behavior.pulse_len = pulse_len;
        behavior.pause_len = pause_len;
        behavior.pulses_in_batch = nPulses;

        Led led;            // system (trouble) LED
        led.ptype = OBJ_ETR;
        led.puin = pAG->change_status_ETR->uin;
        led.suin = 0x01;

        //LED
        if( pAG->change_status_ETR->d.etr.related_groups_qty<2)
        {
            setLedUnitStateALL(&led, UnitStateMultivibrator, &behavior);
        }
        else
        {
            // -- process the reactions list
            processReactions(pAG->events[0], NULL);
        }


#ifndef NO_USE_SYSTEM_LED_AS_ARMED
        // test start main LED
        ArmingGroup_tbl *pGroups = getArmingGroupTable(NULL);
        if(pGroups->size == 1)
        {
            Led *pLed = getLedByParent(getCommonSettings(NULL)->p_masterboard->uin, OBJ_MASTERBOARD, 0x02);
            if (!pLed) return;
            setLedUnitState(pLed, UnitStateMultivibrator, &behavior);
        }
#endif


    }
}


s32 Ar_System_stopPrevETRLed(ArmingGroup *pAG)
{
    // -- get last ETR
    OUT_DEBUG_2("Ar_System_stopPrevETRBuzzerLed() group ID=%d\r\n",
                pAG->id);

    ParentDevice *pPrev_ETR = getParentDeviceByID(pAG->change_status_ETR->id, OBJ_ETR);
    if(!pPrev_ETR){
        OUT_DEBUG_2("Ar_System_stopPrevETRBuzzerLed() prev ETR error\r\n");
    }
    else
    {
        // stop armed led  in last ETR
        ETR *pEtr = &pPrev_ETR->d.etr;
        {
            OUT_DEBUG_2("Ar_System_stopPrevETRBuzzerLed() stop uin=%d\r\n",
                            pPrev_ETR->uin);

                            BehaviorPreset behavior = {0};
                            behavior.pulse_len = 5;
                            behavior.pause_len = 1;
                            behavior.pulses_in_batch = 1;
                            behavior.batch_pause_len= 10;

                            EtrBuzzer buzzer;
                            buzzer.puin = pPrev_ETR->uin;

                            // armed LED
                            Led led;
                            led.ptype = OBJ_ETR;
                            led.puin = pPrev_ETR->uin;
                            led.suin = 0x01;


                            if(pAG->change_status_ETR->d.etr.related_groups_qty<2)
                            {
                                 setLedUnitStateALL(&led, UnitStateOff, &behavior);
                            }
                            else
                            {
                                if (pAG->events[0])
                                {
                                    for (u8 j = 0; j < pAG->events[0]->tbl_subscribers.size; ++j)
                                    {
                                        Reaction *pReaction = pAG->events[0]->tbl_subscribers.t[j];
                                        if(OBJ_RELAY == pReaction->performer_type)
                                        {
                                            Relay *pRelay = getRelayByID(pReaction->performer_id);
                                            if (!pRelay) continue;
                                            setRelayUnitState(pRelay, UnitStateOff, &behavior);
                                        }
                                        if(OBJ_LED == pReaction->performer_type)
                                        {
                                            Led *pLed = getLedByID(pReaction->performer_id);
                                            if (!pLed) continue;
                                            setLedUnitStateALL(pLed, UnitStateOff, &behavior);
                                        }
                                    }
                                }

                            }


#ifndef NO_USE_SYSTEM_LED_AS_ARMED
                            // test start main LED
                            ArmingGroup_tbl *pGroups = getArmingGroupTable(NULL);
                            if(pGroups->size == 1)
                            {
                                Led *pLed = getLedByParent(getCommonSettings(NULL)->p_masterboard->uin, OBJ_MASTERBOARD, 0x02);
                                if (pLed)
                                {
                                setLedUnitState(pLed, UnitStateOff, &behavior);
                                }
                            }
#endif


          }



    }

    // --



}



s32 Ar_System_setPrevETRLed(ArmingGroup *pAG, PerformerUnitState state)
{
    // -- get last ETR
    OUT_DEBUG_2("Ar_System_setPrevETRLed() group ID=%d\r\n",
                pAG->id);

    ParentDevice *pPrev_ETR = getParentDeviceByID(pAG->change_status_ETR->id, OBJ_ETR);
    if(!pPrev_ETR){
        OUT_DEBUG_2("Ar_System_setPrevETRLed() prev ETR error\r\n");
    }
    else
    {
        // stop armed led  in last ETR
        ETR *pEtr = &pPrev_ETR->d.etr;
        {

            OUT_DEBUG_2("uin=%d, state = %d\r\n",
                            pPrev_ETR->uin, state);

                            BehaviorPreset behavior = {0};
                            behavior.pulse_len = 6;
                            behavior.pause_len = 4;
                            behavior.pulses_in_batch = 0;
                            behavior.batch_pause_len= 0;

                            EtrBuzzer buzzer;
                            buzzer.puin = pPrev_ETR->uin;

                            // armed LED
                            Led led;
                            led.ptype = OBJ_ETR;
                            led.puin = pPrev_ETR->uin;
                            led.suin = 0x01;


                            if(pAG->change_status_ETR->d.etr.related_groups_qty<2)
                            {
                                 setLedUnitStateALL(&led, state, &behavior);

                            }
                            else
                            {
                                if (pAG->events[0])
                                {

                                    for (u8 j = 0; j < pAG->events[0]->tbl_subscribers.size; ++j)
                                    {

                                        Reaction *pReaction = pAG->events[0]->tbl_subscribers.t[j];
                                        if(OBJ_RELAY==pReaction->performer_type)
                                        {

                                            Relay *pRelay = getRelayByID(pReaction->performer_id);
                                            if (!pRelay) continue;
                                            setRelayUnitStateAll(pRelay, state, &behavior);
                                        }
                                        if(OBJ_LED==pReaction->performer_type)
                                        {
                                            Led *pLed = getLedByID(pReaction->performer_id);
                                            if (!pLed) continue;
                                            setLedUnitStateALL(pLed, state, &behavior);
                                        }
                                    }
                                }

                            }

#ifndef NO_USE_SYSTEM_LED_AS_ARMED
                            // test start main LED
                            ArmingGroup_tbl *pGroups = getArmingGroupTable(NULL);

                            if(pGroups->size == 1)
                            {
                                Led *pLed = getLedByParent(getCommonSettings(NULL)->p_masterboard->uin, OBJ_MASTERBOARD, 0x02);
                                if (pLed)
                                {
                                    setLedUnitState(pLed, state, &behavior);
                                }
                            }
#endif


          }
    }

    // --



}



void Ar_System_startEtrBuzzer(ArmingGroup *pAG, u8 pulse_len, u8 pause_len, u8 nPulses, bool bIncrementCounter)
{
    OUT_DEBUG_1("Ar_System_startEtrBuzzer() \r\n");
    OUT_DEBUG_1("AG =%d, status=%s \r\n",
                pAG->id,
                pAG->p_group_state->state_name);
    OUT_DEBUG_1("arming delay =%d, disarming delay=%d \r\n",
                pAG->arming_zoneDelay_counter,
                pAG->disarming_zoneDelay_counter);
    OUT_DEBUG_1("ETR =%d, enter counter=%d \r\n",
                pAG->change_status_ETR->uin,
                pAG->change_status_ETR->d.etr.buzzer.counter);
    OUT_DEBUG_1("Increment=%d \r\n", bIncrementCounter);

    if(OBJ_ETR==pAG->change_status_ETR->type){
        BehaviorPreset behavior = {0};
        behavior.pulse_len = pulse_len;
        behavior.pause_len = pause_len;
        behavior.pulses_in_batch = nPulses;

        EtrBuzzer buzzer;
        buzzer.puin =pAG->change_status_ETR->uin;
        // Buzzer

        if(pAG->change_status_ETR->d.etr.buzzer.counter==0)
        {
    OUT_DEBUG_1("Buzzer started \r\n");
            setEtrBuzzerUnitState(&buzzer, UnitStateMultivibrator, &behavior);
        }

        if(bIncrementCounter)
        {
            pAG->change_status_ETR->d.etr.buzzer.counter++;
        }
    }
    OUT_DEBUG_1("ETR =%d, enter counter=%d \r\n",
                pAG->change_status_ETR->uin,
                pAG->change_status_ETR->d.etr.buzzer.counter);
}



s32 Ar_System_stopPrevETRBuzzer(ArmingGroup *pAG)
{
    // -- get last ETR
    OUT_DEBUG_1("Ar_System_stopPrevETRBuzzer() \r\n");
    OUT_DEBUG_1("AG =%d, status=%s \r\n",
                pAG->id,
                pAG->p_group_state->state_name);
    OUT_DEBUG_1("arming delay =%d, disarming delay=%d \r\n",
                pAG->arming_zoneDelay_counter,
                pAG->disarming_zoneDelay_counter);
    OUT_DEBUG_1("ETR =%d, enter counter=%d \r\n",
                pAG->change_status_ETR->uin,
                pAG->change_status_ETR->d.etr.buzzer.counter);

    ParentDevice *pPrev_ETR = getParentDeviceByID(pAG->change_status_ETR->id, OBJ_ETR);
    if(!pPrev_ETR){
        OUT_DEBUG_2("Ar_System_stopPrevETRBuzzerLed() prev ETR error\r\n");
    }
    else
    {
        ETR *pEtr = &pPrev_ETR->d.etr;
        {

                            BehaviorPreset behavior = {0};
                            behavior.pulse_len = 5;
                            behavior.pause_len = 1;
                            behavior.pulses_in_batch = 1;
                            behavior.batch_pause_len= 10;

                            EtrBuzzer buzzer;
                            buzzer.puin = pPrev_ETR->uin;

                            if(pAG->arming_zoneDelay_counter>0 || pAG->disarming_zoneDelay_counter>0)
                            {
                                if(pPrev_ETR->d.etr.buzzer.counter>0)
                                    --pPrev_ETR->d.etr.buzzer.counter;
                                if(pPrev_ETR->d.etr.buzzer.counter==0)
                                {
    OUT_DEBUG_1("Buzzer stopped \r\n");
                                    setEtrBuzzerUnitStateALL(&buzzer, UnitStateOff, &behavior);
                                }
                            }


          }
    }

    // --
    OUT_DEBUG_1("ETR =%d, enter counter=%d \r\n",
                pAG->change_status_ETR->uin,
                pAG->change_status_ETR->d.etr.buzzer.counter);

}



bool Ar_System_isZonesInGroupNormal_Besides_WALK_THROUGH(ArmingGroup *pGroup)
{
    if(!pGroup)
        return FALSE;
    // test zone
    bool bAllEnabledZonesIsNormal=TRUE;
    Zone_tbl *zones_table = getZoneTable(NULL);

    for (u16 pos = 0; pos < zones_table->size; ++pos) {
        if (pGroup->id ==zones_table->t[pos].group_id){
            if (TRUE == zones_table->t[pos].enabled &&
                    ZONE_WALK_THROUGH != zones_table->t[pos].zone_type)
            {
                if (TRUE == zones_table->t[pos].zoneDamage_counter>0)
                {
                        bAllEnabledZonesIsNormal=FALSE;
                }
            }
        }
    }
    return bAllEnabledZonesIsNormal;
}

bool Ar_System_isZonesInGroupNormal(ArmingGroup *pGroup)
{
    if(!pGroup)
        return FALSE;
    // test zone
    bool bAllEnabledZonesIsNormal=TRUE;
    Zone_tbl *zones_table = getZoneTable(NULL);

    for (u16 pos = 0; pos < zones_table->size; ++pos) {
        if (pGroup->id ==zones_table->t[pos].group_id){
            if (TRUE == zones_table->t[pos].enabled)
            {
                if (TRUE == zones_table->t[pos].zoneDamage_counter>0)
                {
                        bAllEnabledZonesIsNormal=FALSE;
                }
            }
        }
    }
    return bAllEnabledZonesIsNormal;
}

void Ar_System_setSystemError(SystemErrorCode code)
{
    OUT_DEBUG_2("Ar_System_setSystemError(code=%d)\r\n", code);

    BehaviorPreset behavior = {0};
    behavior.pulse_len = 5;
    behavior.pause_len = 2;
    behavior.batch_pause_len = 100;    // 10 sec pause between pulse batches
    behavior.pulses_in_batch = code;

    PerformerUnitState state = (SEC_ClearLastError == code) ? UnitStateOff : UnitStateMultivibrator;
    BehaviorPreset *pBehavior = (SEC_ClearLastError == code) ? NULL : &behavior;

    Led led;            // system (trouble) LED
    led.ptype = OBJ_ETR;
    led.puin = 0x00;
    led.suin = 0x04;

    EtrBuzzer buzzer;
    buzzer.puin = 0x00;

    s32 ret = 0;

    ret = setEtrBuzzerUnitState(&buzzer, state, pBehavior);
    if (ret < RETURN_NO_ERRORS)
        OUT_DEBUG_1("setEtrBuzzerUnitState() = %d error\r\n", ret);

    ret = setLedUnitState(&led, state, pBehavior);
    if (ret < RETURN_NO_ERRORS)
        OUT_DEBUG_1("setLedUnitState() = %d error\r\n", ret);


    /* remember behavior of a buzzer and a trouble led on all ETRs */
    ParentDevice_tbl *pParents = getParentDevicesTable(NULL);
    for (u16 i = 0; i < pParents->size; ++i) {
        ParentDevice *pParent = &pParents->t[i];
        if (OBJ_ETR != pParent->type)   // skip non-ETR units
            continue;
        ETR *pEtr = &pParent->d.etr;
        /* buzzer */
        pEtr->buzzer.state = state;
        pEtr->buzzer.behavior_preset = behavior;
        /* trouble led */
        Led *pLed = getLedByParent(pParent->uin, OBJ_ETR, led.suin); // find System (Trouble) led
        pLed->state = state;
        pLed->behavior_preset = behavior;
    }
}

void Ar_System_setFOTA_process_flag(bool value)
{
    bFOTA_process = value;
}

bool Ar_System_isFOTA_process_flag(void)
{
    return  bFOTA_process;
}


void Ar_System_setAudio_flag(bool value)
{
    bAudio = value;
}

bool Ar_System_isAudio_flag(void)
{
    return  bAudio;
}


void Ar_System_setLastVoltage(float value)
{
    last_voltage = value;
}

float Ar_System_getLastVoltage(void)
{
    return  last_voltage;
}


void Ar_System_setPSSFreezeCounter(u16 value)
{
    PSS_Freze_Counter = value;
}

u16 Ar_System_getPSSFreezeCounter(void)
{
    return  PSS_Freze_Counter;
}

void Ar_System_setBellState(PerformerUnitState state)
{
    BehaviorPreset behavior = {0};
    behavior.pulse_len = 1;
    behavior.pause_len = 1;
    behavior.pulses_in_batch = 0;
    behavior.batch_pause_len= 0;

    Bell *pBell = getBellByID(1);

    s32 ret1 = setBellUnitState(pBell, state, &behavior);

}
