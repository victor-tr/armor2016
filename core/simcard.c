#include "atcommand.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "core/simcard.h"
#include "core/uart.h"
#include "core/mcu_tx_buffer.h"
#include "core/timers.h"
#include "core/system.h"

#include "pult/gprs_udp.h"

#include "service/humanize.h"

#include "ril_network.h"


typedef enum {
    SCSS_SWITCHING_STARTED        = 0,
    SCSS_CURRENT_SIM_SLOT_GETTING = 1,
    SCSS_GET_SIM_STATUS           = 2,
    SCSS_SIM_STATUS_GETTING       = 3,
    SCSS_SIM_CHECKED              = 4,
    SCSS_UNREGISTERING            = 5,
    SCSS_UNREGISTERED             = 6,
    SCSS_SWITCHING                = 7,
    SCSS_SWITCHED                 = 8,
    SCSS_REGISTERING              = 9,
    SCSS_REGISTERED               = 10,
    SCSS_FAILED                   = 11,
    SCSS_READY_TO_USE             = 12
} SimSlotSwitchingState;


static SIMSlot _current_simcard = SIM_1;    // previous active simcard
static SIMSlot _new_simcard = SIM_1;    // SIM card that should be activated
static SimSlotSwitchingState _sim_switching_state = SCSS_READY_TO_USE;

static SimSettings *_pCurrentSimSettings = NULL;

static u8 _simcard_blocking_counter[2] = {0};   // the device has two sim cards


static s32  SIM_setCurrentSimSettings(SIMSlot simcard);
static void SIM_setSimSlotSwitchingState(SimSlotSwitchingState state);


// callbacks for AT command module
static void callback_simcard_registering(bool success);
static void callback_simcard_unregistering(bool success);
static void callback_simcard_checkSimCardPresence(bool present);
static void callback_simcard_SimSlotChanged(bool success);
static void callback_simcard_getCurrentSimSlot(u8 currentSlot);

static SimCard_AT_Callback_t _cb =
{
    &callback_simcard_unregistering,
    &callback_simcard_registering,
    &callback_simcard_checkSimCardPresence,
    &callback_simcard_SimSlotChanged,
    &callback_simcard_getCurrentSimSlot
};



void callback_simcard_registering(bool success)
{
    if (success) SIM_setSimSlotSwitchingState(SCSS_REGISTERED);
    else SIM_setSimSlotSwitchingState(SCSS_FAILED);
}

void callback_simcard_unregistering(bool success)
{
    if (success) SIM_setSimSlotSwitchingState(SCSS_UNREGISTERED);
    else SIM_setSimSlotSwitchingState(SCSS_FAILED);
}

void callback_simcard_checkSimCardPresence(bool present)
{
    if (present) SIM_setSimSlotSwitchingState(SCSS_REGISTERED);
    else SIM_setSimSlotSwitchingState(SCSS_FAILED);
}

void callback_simcard_SimSlotChanged(bool success)
{
    OUT_DEBUG_2("callback_simcard_SimSlotChanged(): %s\r\n",
                success ? "OK" : "FAIL");

    if (success) SIM_setSimSlotSwitchingState(SCSS_SWITCHED);
    else SIM_setSimSlotSwitchingState(SCSS_FAILED);
}

void callback_simcard_getCurrentSimSlot(u8 currentSlot)
{
    OUT_DEBUG_2("callback_simcard_getCurrentSimSlot(SIM_switching_state=%d): current slot=SIM_%d\r\n",
                _sim_switching_state, currentSlot+1);

    /* if initialized => ALWAYS forcedly reactivate the SIM card */
    if (Ar_System_checkFlag(SysF_DeviceInitialized) || currentSlot != _new_simcard) {
        SIM_setSimSlotSwitchingState(SCSS_SIM_CHECKED);
    } else {
        SIM_setSimSlotSwitchingState(SCSS_GET_SIM_STATUS);
    }
}


void Ar_SIM_registerCallbacks(void)
{ OUT_DEBUG_2("Ar_SIM_registerCallbacks()\r\n"); register_AT_callbacks(&_cb); }

SimSettings *Ar_SIM_currentSettings(void)
{
    if (!_pCurrentSimSettings)
        _pCurrentSimSettings = &getSimSettingsArray(NULL)[_current_simcard];
    return _pCurrentSimSettings;
}

s32 SIM_setCurrentSimSettings(SIMSlot simcard)
{
    OUT_DEBUG_2("SIM_setCurrentSimSettings(SIM_%d)\r\n", simcard + 1);

    s32 ret = 0;
    _simcard_blocking_counter[simcard] = 0; // reset fail counter for activated sim
    _current_simcard = simcard;
    _pCurrentSimSettings = &getSimSettingsArray(&ret)[simcard];
    return ret;
}


SIMSlot Ar_SIM_currentSlot(void)
{ return _current_simcard; }

SIMSlot Ar_SIM_newSlot(void)
{ return _new_simcard; }

bool Ar_SIM_isSlotActivationFailed(void)
{ return SCSS_FAILED == _sim_switching_state; }

bool Ar_SIM_isSimCardReady(void)
{ return SCSS_READY_TO_USE == _sim_switching_state; }

void SIM_setSimSlotSwitchingState(SimSlotSwitchingState state)
{ _sim_switching_state = state; }


static const char *SIM_getSimSwitchingStateByCode(u8 code)
{
    switch (code) {
    CASE_RETURN_NAME(SCSS_SWITCHING_STARTED       );
    CASE_RETURN_NAME(SCSS_CURRENT_SIM_SLOT_GETTING);
    CASE_RETURN_NAME(SCSS_GET_SIM_STATUS          );
    CASE_RETURN_NAME(SCSS_SIM_STATUS_GETTING      );
    CASE_RETURN_NAME(SCSS_SIM_CHECKED             );
    CASE_RETURN_NAME(SCSS_UNREGISTERING           );
    CASE_RETURN_NAME(SCSS_UNREGISTERED            );
    CASE_RETURN_NAME(SCSS_SWITCHING               );
    CASE_RETURN_NAME(SCSS_SWITCHED                );
    CASE_RETURN_NAME(SCSS_REGISTERING             );
    CASE_RETURN_NAME(SCSS_REGISTERED              );
    CASE_RETURN_NAME(SCSS_FAILED                  );
    CASE_RETURN_NAME(SCSS_READY_TO_USE            );
    DEFAULT_RETURN_CODE(code);
    }
}


void Ar_SIM_ActivateSlot(SIMSlot simcard)
{
    Ar_Timer_stop(TMR_ActivateSimCard_Timer);

    OUT_DEBUG_2("Ar_SIM_ActivateSlot(SIM_%d)\r\n", simcard+1);

    _new_simcard = simcard;
    SIM_setSimSlotSwitchingState(SCSS_SWITCHING_STARTED);
    Ar_Timer_startSingle(TMR_ActivateSimCard_Timer, 1);
}

static s32 SIM_SlotActivation_Step(void)
{
    OUT_DEBUG_2("SIM_SlotActivation_Step(SIM_%d, SIM_switching_state=%s)\r\n",
                _new_simcard+1, SIM_getSimSwitchingStateByCode(_sim_switching_state));

    s32 ret = 0;

    switch (_sim_switching_state) {
    case SCSS_SWITCHING_STARTED:
    {
        if (Ar_System_isSimSlotSwitchingAllowed()) {
            SIM_setSimSlotSwitchingState(SCSS_CURRENT_SIM_SLOT_GETTING);
            ret = send_AT_cmd_by_code(ATCMD_GetCurrentSimSlot);
            if (ret != RIL_AT_SUCCESS) return ret;
        }
        break;
    }

    case SCSS_GET_SIM_STATUS:
    {
        SIM_setSimSlotSwitchingState(SCSS_SIM_STATUS_GETTING);

        s32 simStat = 0;
        RIL_NW_GetSIMCardState(&simStat);
        switch (simStat) {
        case SIM_STAT_READY:
            SIM_setSimSlotSwitchingState(SCSS_REGISTERED);
            break;
        case SIM_STAT_NOT_INSERTED:
            SIM_setSimSlotSwitchingState(SCSS_SIM_CHECKED);
            break;
        default:
            OUT_DEBUG_3("Current SIM card ERROR state: %d\r\n", simStat);
            SIM_setSimSlotSwitchingState(SCSS_SIM_CHECKED);
        }

        break;
    }

    case SCSS_SIM_CHECKED:
    {
        //deactivateGprs();
        SIM_setSimSlotSwitchingState(SCSS_UNREGISTERING);
        ret = send_AT_cmd_by_code(ATCMD_UnregisterSimCard);
        if (ret != RIL_AT_SUCCESS) return ret;
        break;
    }

    case SCSS_UNREGISTERED:
    {
        SIM_setSimSlotSwitchingState(SCSS_SWITCHING);
        ATCommandIdx cmdcode = (SIM_1 == _new_simcard) ? ATCMD_ActivateSim1 : ATCMD_ActivateSim2;
        ret = send_AT_cmd_by_code(cmdcode);
        if (ret != RIL_AT_SUCCESS) return ret;
        break;
    }

    case SCSS_SWITCHED:
    {
        SIM_setSimSlotSwitchingState(SCSS_REGISTERING);
        ret = send_AT_cmd_by_code(ATCMD_RegisterSimCard);
        if (ret != RIL_AT_SUCCESS) return ret;
        break;
    }

    case SCSS_REGISTERED:
        ret = SIM_setCurrentSimSettings(_new_simcard);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("SIM_setCurrentSimSettings() = %d error\r\n", ret);
            return ret;
        }
        OUT_DEBUG_3("SIM_%d activated successfully\r\n", _new_simcard + 1);
        SIM_setSimSlotSwitchingState(SCSS_READY_TO_USE);
        return RETURN_NO_ERRORS;

    case SCSS_READY_TO_USE:
        return RETURN_NO_ERRORS;

    case SCSS_FAILED:
        OUT_DEBUG_1("SIM_%d activation FAILED\r\n", _new_simcard + 1);
        return ERR_SIM_ACTIVATION_FAILED;

    default:
        break;
    }

    return Ar_Timer_startSingle(TMR_ActivateSimCard_Timer, SWITCH_SIMCARD_TIMER_INTERVAL);
}

void Ar_SIM_activateSlot_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_ActivateSimCard_Timer));

    s32 ret = SIM_SlotActivation_Step();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("SIM_SlotActivation_Step(SIM_%d) = %d error\r\n", _new_simcard + 1, ret);
    }
}


bool Ar_SIM_canSlotBeUsed(SIMSlot simcard)
{
    return getSimSettingsArray(NULL)[simcard].can_be_used
                && (_simcard_blocking_counter[simcard] < SIMCARD_BLOCKING_THRESHOLD);
}

void Ar_SIM_markSimCardFailed(SIMSlot simcard)
{
    OUT_DEBUG_2("Ar_SIM_markSimCardFailed(SIM_%d)\r\n", simcard+1);
    if (_simcard_blocking_counter[simcard] < SIMCARD_BLOCKING_THRESHOLD)
        ++_simcard_blocking_counter[simcard];
}

void Ar_SIM_blockSlot(SIMSlot simcard)
{
    OUT_DEBUG_2("Ar_SIM_blockSlot(SIM_%d)\r\n", simcard+1);
    _simcard_blocking_counter[simcard] = SIMCARD_BLOCKING_THRESHOLD;
}
