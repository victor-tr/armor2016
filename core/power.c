#include "power.h"

#include "ql_adc.h"
#include "ql_fs.h"


#include "ril.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "db/lexicon.h"

#include "pult/pult_tx_buffer.h"
#include "pult/pult_requests.h"

#include "core/mcudefinitions.h"
#include "core/system.h"

#include "service/humanize.h"

#include "configurator/configurator.h"
#include "configurator/configurator_tx_buffer.h"

#include "states/armingstates.h"


#define BAT_ADC_PIN  PIN_ADC0
#define GET_ABS(x)   ((x) > 0 ? (x) : 0)

#define CHARGING_WAIT_THRESHOLD  (u32)(BATTERY_CHARGING_INTERVAL / ADC_SAMPLING_INTERVAL)


typedef enum {
    Battery_Undefined,
    Battery_MissingOrDead,
    Battery_CriticalLow,
    Battery_Low,
    Battery_Normal,
    Battery_High
} BatteryPower;

static BatteryPower _battery_state = Battery_Undefined;


typedef enum _PowerFlag {
    PF_AC_Ok                 = 0x01,    // AC power is OK
    PF_Battery_Charging      = 0x02,
    PF_Read_ADC_Value        = 0x04,    // calibration of the ADC is in progress
    PF_LowSystemBatteryReported = 0x08
} PowerFlag;

static u8 _power_flags = 0;
static u8 _measuring_counter = 0;

static bool bAC_Ok = TRUE;



//**************************************************************
//*   Prototypes
//**************************************************************
static void power_setBatteryState(const BatteryPower state);
static inline void power_setFlag(PowerFlag flag, bool state);
static inline bool power_checkFlag(PowerFlag flag);
static void power_updateEtrIndication(void);
static void power_updateChargingRelay(void);
static const char *power_batteryStateByCode(BatteryPower code);
static bool power_suppressValueDeviations(u16 *value);
static BatteryPower power_evaluateBatteryState(float voltage);
static void power_process_ADC_data(u16 adc_value);
static void power_callback_ADC(Enum_ADCPin adcPin, u32 adcValue, void *data);
//**************************************************************



//**************************************************************
//*   Internal
//**************************************************************
void power_setBatteryState(const BatteryPower state)
{
    if (state == _battery_state)
        return;

    OUT_DEBUG_2("power_setBatteryState()\r\n");

    // -- previous state
    switch (_battery_state) {
    case Battery_High:
    {

        break;
    }
    case Battery_Normal:
    {

        break;
    }
    case Battery_Low:
    {

        break;
    }
    case Battery_CriticalLow:
    {

        break;
    }
    case Battery_MissingOrDead:
    {
        if (_battery_state < state) {
            resetFlagSystemErrorCodeInBuffer(SEC_Error2_battery_dead);
//            Ar_System_setSystemError(SEC_ClearLastError);
            reportToPult(0, 0, PMC_BatteryMissingOrDead, PMQ_RestoralOrSetArmed);
        }
        break;
    }
    default:
        break;
    }

    // -- new state
    switch (state) {
    case Battery_High:
    {
        power_setFlag(PF_Battery_Charging, FALSE);
        break;
    }
    case Battery_Normal:
    {
        /*
         *  It's needed if previous state was Battery_MissingOrDead
         *  where the Battery was not charging
         */
        if (state > _battery_state)
            power_setFlag(PF_Battery_Charging, TRUE);

        if (power_checkFlag(PF_LowSystemBatteryReported)) {
            power_setFlag(PF_LowSystemBatteryReported, FALSE);
            reportToPult(0, 0, PMC_LowSystemBattery, PMQ_RestoralOrSetArmed);
        }
        break;
    }
    case Battery_Low:
    {
        power_setFlag(PF_Battery_Charging, TRUE);
        break;
    }
    case Battery_CriticalLow:
    {
        reportToPult(0, 0, PMC_LowSystemBattery, PMQ_NewMsgOrSetDisarmed);
        power_setFlag(PF_LowSystemBatteryReported|PF_Battery_Charging, TRUE);

        // -- prepare the device power off
        s32 ret = Ar_System_aboutToPowerOff(0);
        if (ret < RETURN_NO_ERRORS)
            OUT_DEBUG_1("Ar_System_aboutToPowerOff() = %d error\r\n", ret);
        break;
    }
    case Battery_MissingOrDead:
    {
        setFlagSystemErrorCodeInBuffer(SEC_Error2_battery_dead);
//        Ar_System_setSystemError(SEC_Error2_battery_dead);
        reportToPult(0, 0, PMC_BatteryMissingOrDead, PMQ_NewMsgOrSetDisarmed);
        break;
    }
    default:
        break;
    }

    // -- set battery state
    _battery_state = state;

    // -- update indication according to current battery state
    power_updateEtrIndication();
}

static inline void power_setFlag(PowerFlag flag, bool state)
{ _power_flags = state ? (_power_flags | flag) : (_power_flags & ~flag); }

static inline bool power_checkFlag(PowerFlag flag)
{ return (_power_flags & flag) == flag; }

static inline void power_resetFlags(void)
{ _power_flags = 0; }

void power_updateEtrIndication(void)
{
    Led_tbl *pLeds = getLedTable(0);
    if (!pLeds)
        return;

    bool b_AC_ok = power_checkFlag(PF_AC_Ok);

    for (u16 i = 0; i < pLeds->size; ++i)
    {
        Led *pLed = &pLeds->t[i];
        if (OBJ_ETR != pLed->ptype)
            continue;

        switch (pLed->suin) {
        case 0x02:             // AC power led
        {
            setLedUnitState(pLed, b_AC_ok ? UnitStateOn : UnitStateOff, NULL);
            break;
        }
        case 0x03:             //  battery led
        {
            BehaviorPreset preset = {0};
            switch (_battery_state) {
            case Battery_High:
            case Battery_Normal:
                setLedUnitState(pLed, b_AC_ok ? UnitStateOff : UnitStateOn, NULL);
                break;
            case Battery_Low:
                preset.pulse_len = 5; // 0.5 sec meandr
                preset.pause_len = 5;
                setLedUnitState(pLed, UnitStateMultivibrator, &preset);
                break;
            case Battery_MissingOrDead:
            case Battery_CriticalLow:
                preset.pulse_len = 2; // 0.2 sec meandr
                preset.pause_len = 2;
                setLedUnitState(pLed, UnitStateMultivibrator, &preset);
                break;
            default:
                break;
            }
            break;
        }
        }
    }
}

void Ar_power_updateEtrIndication(void)
{
    power_updateEtrIndication();
}

void power_updateChargingRelay(void)
{
    Relay *pChargingRelay = getRelayByParent(getCommonSettings(0)->p_masterboard->uin,
                                        OBJ_MASTERBOARD,
                                        0x01);
    if (!pChargingRelay)
        return;

    bool bNeedCharge = FALSE;

    switch (_battery_state) {
    case Battery_Normal:
        if (power_checkFlag(PF_Battery_Charging))
            bNeedCharge = TRUE;
        break;
    case Battery_Low:
    case Battery_CriticalLow:
        bNeedCharge = TRUE;
        break;
    default:
        break;
    }

    setRelayUnitState(pChargingRelay,
                      power_checkFlag(PF_AC_Ok) && bNeedCharge ? UnitStateOn : UnitStateOff,
                      NULL);
}

const char *power_batteryStateByCode(BatteryPower code)
{
    switch (code) {
    CASE_RETURN_NAME(Battery_Undefined);
    CASE_RETURN_NAME(Battery_High);
    CASE_RETURN_NAME(Battery_Normal);
    CASE_RETURN_NAME(Battery_Low);
    CASE_RETURN_NAME(Battery_CriticalLow);
    CASE_RETURN_NAME(Battery_MissingOrDead);
    DEFAULT_RETURN_CODE(code);
    }
}

// return: true - suppressed, false - unsuppressed (exceeded the delta)
bool power_suppressValueDeviations(u16 *value)
{
    static u16 last_value = 0;
    const u16 delta = last_value * ADC_DELTA_PERCENT / 100;

    if (*value > last_value + delta ||
            *value < last_value - delta)
    {
        last_value = *value;
        return FALSE;
    } else {
        *value = last_value;
        return TRUE;
    }
}

BatteryPower power_evaluateBatteryState(float voltage)
{
    BatteryPower current_state = Battery_Undefined;

    if (voltage >= BAT_HI) {
        current_state = Battery_High;
    }
    else if (voltage < BAT_HI && voltage > BAT_LO) {
        current_state = Battery_Normal;
    }
    else if (voltage <= BAT_LO && voltage > BAT_CRITICAL) {
        current_state = Battery_Low;
    }
    else if (voltage <= BAT_CRITICAL && voltage > BAT_DEAD) {
        current_state = Battery_CriticalLow;
    }
    else /* if (voltage <= BAT_DEAD) */ {
        current_state = Battery_MissingOrDead;
    }

    return current_state;
}

void power_process_ADC_data(u16 adc_value)
{
    static BatteryPower previous_battery_state = Battery_Undefined;

    CommonSettings *pCommonSettings = getCommonSettings(0);

    power_suppressValueDeviations(&adc_value);
    float current_voltage = GET_ABS(adc_value - pCommonSettings->adc_b) / pCommonSettings->adc_a;

    // -- save current voltage
    Ar_System_setLastVoltage(current_voltage);



    BatteryPower current_battery_state = power_evaluateBatteryState(current_voltage);

    if (previous_battery_state != current_battery_state) {
        previous_battery_state = current_battery_state;
        _measuring_counter = 0;
    }


    /* the threshold to make a decision */
    if (++_measuring_counter >= 5) {
        _measuring_counter = 0;

        // -- not apply any reactions, only send ADC response to configurator
        if (power_checkFlag(PF_Read_ADC_Value))
        {
            power_setFlag(PF_Read_ADC_Value, FALSE);
            Ar_Power_sendAdcResponse(adc_value);
        }
        else
        {
            // -- process battery state
            power_setBatteryState(current_battery_state);
            // -- control the charging relay
            power_updateChargingRelay();
        }

    OUT_UNPREFIXED_LINE("\r\n");

#ifndef NO_POWER_OUT_DEBUG
        Relay *pChargingRelay = getRelayByParent(pCommonSettings->p_masterboard->uin,
                                                 OBJ_MASTERBOARD,
                                                 0x01);

        /* FIXME: stupid conversion of a float type data to two integer values;
                  used here because an '%.1f' qualifier doesn't recognized by the vsprintf() func
                  (and now I don't understand this issue),
                  see the OUT_DEBUG_8 macro for more details */
        OUT_DEBUG_8("power_process_ADC_data( state=%s, ADC=%d, Voltage=%d.%dV, charging=%s )\r\n",
                    power_batteryStateByCode(current_battery_state),
                    adc_value,
                    (u8)current_voltage,    // integer part of the float voltage
                    (u8)((current_voltage - (u8)current_voltage) * 10), // fractional part of the float voltage
                    (UnitStateOn == pChargingRelay->state) ? "Yes" : "No");
#endif



#ifndef NO_PERIODICAL_BLOCK
        //OUT_DEBUG_8("TEST BLOCK (NO_PERIODICAL_BLOCK in DEFINES.H)\r\n");

        ST_Time time;
        Ql_GetLocalTime(&time);
        u32 LargeSec = time.hour * 60 * 60 + time.minute * 60 + time.second;

//        OUT_DEBUG_8("Time = %d:%d:%d (%d)\r\n",time.hour, time.minute, time.second, LargeSec);



//        u32 mem_total_ufs = Ql_FS_GetTotalSpace(1);
//        u32 mem_total_ram = Ql_FS_GetTotalSpace(3);

//        OUT_DEBUG_8("Mem total UFS = %d, RAM = %d \r\n",mem_total_ufs, mem_total_ram);

//        mem_total_ufs = Ql_FS_GetFreeSpace(1);
//        mem_total_ram = Ql_FS_GetFreeSpace(3);

//        OUT_DEBUG_8("Mem free UFS = %d, RAM = %d \r\n",mem_total_ufs, mem_total_ram);



        //-----------------------------------------------------------------------


#endif

    }
}

void power_callback_ADC(Enum_ADCPin adcPin, u32 adcValue, void *data)
{
//    OUT_DEBUG_2("power_callback_ADC(adcPin=%d)\r\n", adcPin);

    Relay *pBatteryRelay = getRelayByParent(getCommonSettings(0)->p_masterboard->uin,
                                            OBJ_MASTERBOARD,
                                            0x01);
    if (!pBatteryRelay)
        return;

    if (BAT_ADC_PIN == adcPin)
    {
        static u32 _charging_wait_interval = 0;

        if (UnitStateOff != pBatteryRelay->state)
        {
            if (++_charging_wait_interval >= CHARGING_WAIT_THRESHOLD ||
                    power_checkFlag(PF_Read_ADC_Value) ||
                    !power_checkFlag(PF_AC_Ok))
            {
                _charging_wait_interval = 0;
                s32 ret = setRelayUnitState(pBatteryRelay, UnitStateOff, 0);
                if (ret < RETURN_NO_ERRORS)
                    OUT_DEBUG_1("setRelayUnitState() = %d error\r\n", ret);
            }
        }
        else
        {
            power_process_ADC_data(adcValue);
        }
    }
}



//**************************************************************
//*   Interface
//**************************************************************
s32 Ar_Power_processPowerSourceMsg(u8 *pkt)
{
    if (Ar_System_checkFlag(SysF_DeviceInitialized))
    {
        // -- remember power state
        const char *recv_lex = (char *)&pkt[MCUPKT_BEGIN_DATABLOCK+1];

        if (Ar_Lexicon_isEqual(recv_lex, lexicon.source.out.EvtFalse)) {
            power_setFlag(PF_AC_Ok, FALSE);
            bAC_Ok = FALSE;
        }
        else if (Ar_Lexicon_isEqual(recv_lex, lexicon.source.out.EvtTrue)) {
            power_setFlag(PF_AC_Ok, TRUE);
            bAC_Ok = TRUE;
        }
        else {
            OUT_DEBUG_1("Ar_Power_processPowerSourceMsg(): Invalid lexicon. Abort.\r\n");
            return RETURN_NO_ERRORS;
        }

        const bool b_AC_ok = power_checkFlag(PF_AC_Ok);

        power_updateEtrIndication();
        power_updateChargingRelay();

        return reportToPult(0, 0, PMC_AC_Loss,
                             b_AC_ok ? PMQ_RestoralOrSetArmed : PMQ_NewMsgOrSetDisarmed);
    }
    else{

        // -- remember power state
        const char *recv_lex = (char *)&pkt[MCUPKT_BEGIN_DATABLOCK+1];

        if (Ar_Lexicon_isEqual(recv_lex, lexicon.source.out.EvtFalse)) {
            bAC_Ok = FALSE;
            power_setFlag(PF_AC_Ok, FALSE);
        }
        else if (Ar_Lexicon_isEqual(recv_lex, lexicon.source.out.EvtTrue)) {
            bAC_Ok = TRUE;
            power_setFlag(PF_AC_Ok, TRUE);
        }

        return RETURN_NO_ERRORS;

    }
}


void Ar_Power_registerADC(void)
{
    OUT_DEBUG_2("Ar_Power_registerADC()\r\n");

    s32 ret = Ql_ADC_Register(BAT_ADC_PIN, &power_callback_ADC, NULL);
    if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_ADC_Register() = %d error\r\n", ret);
        return;
    }

    ret = Ql_ADC_Init(BAT_ADC_PIN, 5, 200 * ADC_SAMPLING_INTERVAL); // sampling period: 5 times * 200 ms = 1 sec
    if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_ADC_Init() = %d error\r\n", ret);
        return;
    }
}

void Ar_Power_enableBatterySampling(bool enable)
{
    OUT_DEBUG_2("Ar_Power_enableBatterySampling(%s)\r\n", enable ? "TRUE" : "FALSE");

    if (enable) {
        /* reset battery state */
        _measuring_counter = 0;
        _battery_state = Battery_Undefined;

        // -- initialize flags
        power_resetFlags();
//        power_setFlag(PF_AC_Ok, TRUE);
        power_setFlag(PF_AC_Ok, bAC_Ok);
    }

    s32 ret = Ql_ADC_Sampling(BAT_ADC_PIN, enable);
    if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_ADC_Sampling() = %d error\r\n", ret);
    }
}


void Ar_Power_requestAdcData()
{
    OUT_DEBUG_2("Ar_Power_requestAdcData()\r\n");
    power_setFlag(PF_Read_ADC_Value, TRUE);

    /* must perform full measuring cycle since ADC calibration was requested */
    _measuring_counter = 0;
}

s32 Ar_Power_sendAdcResponse(u16 adcValue)
{
    OUT_DEBUG_2("Ar_Power_sendAdcResponse(value=%d)\r\n", adcValue);

    u8 adcResponse[ADCPKT_MAX] = {0};

    adcResponse[ADCPKT_VALUE_H] = adcValue >> 8;
    adcResponse[ADCPKT_VALUE_L] = adcValue;

    fillTxBuffer_configurator(PC_READ_ADC, FALSE, adcResponse, sizeof(adcResponse));
    return sendBufferedPacket_configurator();
}


bool Ar_Power_startFlag(){
    return bAC_Ok;
}
