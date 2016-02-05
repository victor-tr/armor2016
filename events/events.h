#ifndef EVENTS_H
#define EVENTS_H


//************************************************************************
//* GROUP STATES
//************************************************************************/

typedef enum _GroupStateIdent {
    STATE_DISARMED,
    STATE_DISARMED_FORCED,
    STATE_DISARMED_ING,
    STATE_ARMED,
    STATE_ARMED_PARTIAL,
    STATE_ARMED_ING,
    STATE_CONFIGURING,

    STATE_MAX
} GroupStateIdent;


//************************************************************************
//* BUTTON
//************************************************************************/
typedef enum _ButtonEventType {
    BUTTON_RELEASED     = 0,
    BUTTON_PRESSED      = 1,
    BUTTON_PRESSED_LONG = 2
} ButtonEventType;


//************************************************************************
//* ZONE
//************************************************************************/
typedef enum _ZoneEventType {
    ZONE_EVENT_NORMAL   = 0,
    ZONE_EVENT_BREAK    = 1,
    ZONE_EVENT_FITTING  = 2,
    ZONE_EVENT_SHORT    = 3,
    ZONE_EVENT_DISABLED = 8
} ZoneEventType;


//************************************************************************
//* OTHER EVENTS
//************************************************************************/
typedef enum _SystemEventType {
    ARMING_EVENT                 = 0,
    COMMON_KEY_EVENT             = 1,
    INTERNAL_TIMER_EVENT_TIMEOUT = 2,
    DELAYEDZONE_TIMER_EVENT_TIMEOUT = 3,
    GROUP_EVENT_STATE_CHANGED_DISARMED_TO_ARMED = 4,
    GROUP_EVENT_STATE_CHANGED_ARMED_TO_DISARMED = 5,
} SystemEventType;

#endif // EVENTS_H
