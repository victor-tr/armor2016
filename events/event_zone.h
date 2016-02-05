/*
 * events.h
 *
 * Created: 18.10.2012 10:17:37
 *  Author: user
 */


#ifndef EVENT_ZONE_H
#define EVENT_ZONE_H

#include "ql_type.h"
#include "ql_time.h"

#include "db/db.h"

#include "events.h"


//typedef struct _ZoneEntryDelay {
    //int number;
    //double interval;
    //int repeats;
//} ZoneEntryDelay;


// TODO: add here struct's declarations for each zone type
// and than append these types to the ZoneData union.


typedef struct _ZoneEvent {
// -- from MCU
    ZoneEventType zone_event_type;

// -- after parsing
    Zone *p_zone;

// -- when and in what state the event has occured
    ST_Time      time;
    GroupState  *p_current_arming_state;
    ArmingGroup *p_group;
} ZoneEvent;


s32 process_mcu_zone_msg(u8 *pkt);



#endif /* EVENT_ZONE_H */
