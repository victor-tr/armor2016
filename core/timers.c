#include "timers.h"

#include "configurator/configurator.h"

#include "events/event_key.h"

#include "pult/connection.h"
#include "pult/gprs_udp.h"

#include "core/mcu_tx_buffer.h"
#include "core/system.h"
#include "core/simcard.h"

#include "service/humanize.h"


#define TIMER_IDX(timerId)    (timerId - TMR_START)


u32 timer_mutex;

static s32 _timeout_datas[TMR_QTY] = {0};
static bool _timer_is_active[TMR_QTY] = {FALSE};


const char *Ar_Timer_getTimerNameByCode(u16 timerId)
{
    switch (timerId) {
    CASE_RETURN_NAME(TMR_McuCtsFlag_Timer);
    CASE_RETURN_NAME(TMR_StartupInit_Timer);
    CASE_RETURN_NAME(TMR_ConfiguratorNoResponse_Timer);
    CASE_RETURN_NAME(TMR_ActivateGPRS_Timer);
    CASE_RETURN_NAME(TMR_ActivateSimCard_Timer);
    CASE_RETURN_NAME(TMR_PultSession_Timer);
    CASE_RETURN_NAME(TMR_PultProtocol_Timer);
    CASE_RETURN_NAME(TMR_PultRxQueue_Timer);
    CASE_RETURN_NAME(TMR_SoftwareTimersSet);
    DEFAULT_RETURN_CODE(timerId);
    }
}


static void callback_onTimer(u32 timerId, void *param)
{
    if (TMR_SoftwareTimersSet != timerId)   // to void inserting a space when software timers processed
        OUT_UNPREFIXED_LINE("\r\n");

    _LOCK_TIMER_MUTEX
    _timer_is_active[TIMER_IDX(timerId)] = FALSE;
    _RELEASE_TIMER_MUTEX

    switch (timerId) {
    case TMR_ActivateGPRS_Timer:
        activateGprs_timerHandler(param);
        break;
    case TMR_ActivateSimCard_Timer:
        Ar_SIM_activateSlot_timerHandler(param);
        break;
    case TMR_ConfiguratorNoResponse_Timer:
        Ar_Configurator_NoResponse_timerHandler(param);
        break;
    case TMR_McuCtsFlag_Timer:
        mcu_cts_flag_timerHandler(param);
        break;
    case TMR_PultProtocol_Timer:
    {
        IProtocol *protocol = getProtocol(getChannel()->type);
        if (protocol && protocol->timerHandler)
            protocol->timerHandler(param);
        break;
    }
    case TMR_PultSession_Timer:
        pultSession_timerHandler(param);
        break;
    case TMR_StartupInit_Timer:
        System_startupInit_timerHandler(param);
        break;
    case TMR_PultRxQueue_Timer:
        pultRxBuffer_timerHandler(param);
        break;

    case TMR_SoftwareTimersSet:
        Ar_System_processSoftwareTimers(param);
        break;

    default:
        OUT_DEBUG_1("callback_onTimer(): unregistered timerId = %#x\r\n", timerId)
    }
}

void Ar_Timer_registerTimers(void)
{
    OUT_DEBUG_2("Ar_Timer_registerTimers()\r\n")

    _timeout_datas[TIMER_IDX(TMR_ActivateGPRS_Timer)]           = 0;
    _timeout_datas[TIMER_IDX(TMR_ActivateSimCard_Timer)]        = 0;
    _timeout_datas[TIMER_IDX(TMR_PultProtocol_Timer)]           = 0;
    _timeout_datas[TIMER_IDX(TMR_ConfiguratorNoResponse_Timer)] = 0;
    _timeout_datas[TIMER_IDX(TMR_McuCtsFlag_Timer)]             = 0;
    _timeout_datas[TIMER_IDX(TMR_PultSession_Timer)]            = 0;
    _timeout_datas[TIMER_IDX(TMR_StartupInit_Timer)]            = 0;
    _timeout_datas[TIMER_IDX(TMR_PultRxQueue_Timer)]            = 0;
    _timeout_datas[TIMER_IDX(TMR_SoftwareTimersSet)]            = 0;

    s32 ret = 0;
    for (u8 i = 0; i < TMR_QTY; ++i) {
        ret = Ql_Timer_Register(i + TMR_START, callback_onTimer, &_timeout_datas[i]);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Ql_Timer_Register(%s, id=%#x) = %d error\r\n", Ar_Timer_getTimerNameByCode(i+TMR_START), i + TMR_START, ret);
        } else {
            OUT_DEBUG_8("Ql_Timer_Register(%s, id=%#x): OK\r\n", Ar_Timer_getTimerNameByCode(i+TMR_START), i + TMR_START);
        }
    }
}


static inline s32 _stopTimer_unlocked(ArmorTimer timerId)
{
    if (!_timer_is_active[TIMER_IDX(timerId)])
        return RETURN_NO_ERRORS;

    s32 ret = Ql_Timer_Stop(timerId);
    if (QL_RET_ERR_INVALID_TIMER == ret) {
        ret = Ql_OS_SendMessage(main_task_id, MSG_ID_STOP_TIMER, timerId, 0);
        if (ret != OS_SUCCESS) { OUT_DEBUG_1("Ql_OS_SendMessage() = %d Enum_OS_ErrCode\r\n", ret); }
    }
    else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_Timer_Stop(%s) = %d error\r\n",
                    Ar_Timer_getTimerNameByCode(timerId), ret);
    }
    else {
        _timer_is_active[TIMER_IDX(timerId)] = FALSE;
        if(timerId > TMR_McuCtsFlag_Timer){
        OUT_DEBUG_7("Ql_Timer_Stop(%s) OK\r\n",
                    Ar_Timer_getTimerNameByCode(timerId));
        }

    }
    return ret;
}


s32 Ar_Timer_startSingle(ArmorTimer timerId, u32 interval_ms)
{
    _LOCK_TIMER_MUTEX
    _stopTimer_unlocked(timerId);
    if (0 == interval_ms) interval_ms = 1;
    s32 ret = Ql_Timer_Start(timerId, interval_ms, FALSE);
    if (QL_RET_ERR_INVALID_TIMER == ret) {
        ret = Ql_OS_SendMessage(main_task_id, MSG_ID_START_SINGLE_TIMER, timerId, interval_ms);
        if (ret != OS_SUCCESS) { OUT_DEBUG_1("Ql_OS_SendMessage() = %d Enum_OS_ErrCode\r\n", ret); }
    }
    else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_Timer_Start(%s, interval=%d ms) = %d error\r\n",
                    Ar_Timer_getTimerNameByCode(timerId), interval_ms, ret);
    }
    else {
        _timer_is_active[TIMER_IDX(timerId)] = TRUE;
        if(timerId > TMR_McuCtsFlag_Timer){
        OUT_DEBUG_7("Ql_Timer_Start(%s, interval=%d ms) OK\r\n",
                    Ar_Timer_getTimerNameByCode(timerId), interval_ms);
        }

    }
    _RELEASE_TIMER_MUTEX
    return ret;
}


s32 Ar_Timer_startContinuous(ArmorTimer timerId, u32 interval_ms)
{
    _LOCK_TIMER_MUTEX
    _stopTimer_unlocked(timerId);
    if (0 == interval_ms) interval_ms = 1;
    s32 ret = Ql_Timer_Start(timerId, interval_ms, TRUE);
    if (QL_RET_ERR_INVALID_TIMER == ret) {
        ret = Ql_OS_SendMessage(main_task_id, MSG_ID_START_CONTINUOUS_TIMER, timerId, interval_ms);
        if (ret != OS_SUCCESS) { OUT_DEBUG_1("Ql_OS_SendMessage() = %d Enum_OS_ErrCode\r\n", ret); }
    }
    else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_Timer_Start(%s, interval=%d ms) = %d error\r\n",
                    Ar_Timer_getTimerNameByCode(timerId), interval_ms, ret);
    }
    else {
        _timer_is_active[TIMER_IDX(timerId)] = TRUE;
        if(timerId > TMR_McuCtsFlag_Timer){
        OUT_DEBUG_7("Ar_Timer_startContinuous(%s, interval=%d ms) OK\r\n",
                    Ar_Timer_getTimerNameByCode(timerId), interval_ms);
        }
    }
    _RELEASE_TIMER_MUTEX
    return ret;
}


s32 Ar_Timer_stop(ArmorTimer timerId)
{
    _LOCK_TIMER_MUTEX
    s32 ret = _stopTimer_unlocked(timerId);
    _RELEASE_TIMER_MUTEX
    return ret;
}
