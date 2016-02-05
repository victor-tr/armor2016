#ifndef EVENT_KEY_H
#define EVENT_KEY_H

#include "ql_type.h"
#include "ql_time.h"
#include "events.h"
#include "db/db.h"


typedef struct {
    DbObjectCode  key_type;
    void         *key;
} KeyInfo;

typedef struct _ArmingEvent {
    DbObjectCode  emitter_type;
    bool          force;
    ArmingGroup  *p_group;

    KeyInfo       key_info;

// -- when and in what state the event has occured
    ST_Time       time;
    GroupState   *p_current_arming_state;
    ParentDevice *p_etr;
} ArmingEvent;

typedef struct _CommonKeyEvent {
    DbObjectCode  emitter_type;
    bool          force;

    KeyInfo       key_info;

// -- when the event occured
    ST_Time       time;

// -- the event can has a related group
    ArmingGroup  *p_group;
    GroupState   *p_current_arming_state;
} CommonKeyEvent;


s32 process_mcu_touchmemory_key_msg(u8 *pkt);
s32 global_handler_common_key(CommonKeyEvent *event);


#endif // EVENT_KEY_H
