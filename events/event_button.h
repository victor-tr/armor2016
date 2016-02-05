#ifndef EVENT_BUTTON_H
#define EVENT_BUTTON_H

#include "ql_type.h"
#include "ql_time.h"

#include "db/db.h"

#include "events.h"


typedef struct _ButtonEvent {
// -- from MCU
    ButtonEventType button_event_type;

// -- after parsing
    Button *p_button;

// -- when and in what state the event has occured
    ST_Time     time;
    GroupState *p_current_arming_state;
} ButtonEvent;

s32 process_mcu_button_msg(u8 *pkt);

s32 common_handler_button(ButtonEvent *event);


#endif // EVENT_BUTTON_H
