#ifndef EVENT_INTERNAL_TIMER_H
#define EVENT_INTERNAL_TIMER_H

#include "ql_type.h"
#include "ql_time.h"
#include "db/db.h"
#include "events.h"


typedef struct _InternalTimerEvent {
// -- from MCU


// -- after parsing


// -- when and in what state the event has occured
    ST_Time     time;
    GroupState *p_current_arming_state;
} InternalTimerEvent;


s32 process_internal_timer_msg(u8 *pkt);


#endif // EVENT_INTERNAL_TIMER_H
