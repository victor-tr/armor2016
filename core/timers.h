#ifndef TIMERS_H
#define TIMERS_H

#include "ql_timer.h"


typedef enum {
    TMR_START               = TIMER_ID_USER_START,

    TMR_McuCtsFlag_Timer    = TMR_START,
    TMR_StartupInit_Timer,
    TMR_ConfiguratorNoResponse_Timer,
    TMR_ActivateGPRS_Timer,
    TMR_ActivateSimCard_Timer,
    TMR_PultSession_Timer,
    TMR_PultProtocol_Timer,
    TMR_PultRxQueue_Timer,
    TMR_SoftwareTimersSet,

    TMR_END,
    TMR_QTY = TMR_END - TMR_START,
} ArmorTimer;


const char *Ar_Timer_getTimerNameByCode(u16 timerId);
void Ar_Timer_registerTimers(void);

s32 Ar_Timer_startSingle(ArmorTimer timerId, u32 interval_ms);
s32 Ar_Timer_startContinuous(ArmorTimer timerId, u32 interval_ms);
s32 Ar_Timer_stop(ArmorTimer timerId);


extern u32 timer_mutex;
#define USE_TIMER_MUTEX

#ifdef USE_TIMER_MUTEX
#define REGISTER_TIMER_MUTEX   timer_mutex = Ql_OS_CreateMutex("timer_mutex");
#define _LOCK_TIMER_MUTEX      Ql_OS_TakeMutex(timer_mutex);
#define _RELEASE_TIMER_MUTEX   Ql_OS_GiveMutex(timer_mutex);
#else
#define REGISTER_TIMER_MUTEX
#define _LOCK_TIMER_MUTEX
#define _RELEASE_TIMER_MUTEX
#endif


#endif // TIMERS_H
