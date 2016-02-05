#ifndef ARMING_STATES_H_
#define ARMING_STATES_H_

#include "ql_type.h"

#include "db/db.h"
#include "db/db_serv.h"

#include "events/events.h"
#include "events/event_zone.h"
#include "events/event_key.h"
#include "events/event_button.h"
#include "events/event_internal_timer.h"


//************************************************************************
//* ARMING STATES
//************************************************************************/

typedef struct _GroupState {
    GroupStateIdent state_ident;
    char *state_name;

// -- handlers
    s32 (* handler_zone)       (ZoneEvent *event);
    s32 (* handler_button)     (ButtonEvent *event);
    s32 (* handler_arming)     (ArmingEvent *event);
    s32 (* handler_common_key) (CommonKeyEvent *event);
    s32 (* handler_timer)      (InternalTimerEvent *event);
} GroupState;


// -------------------------------------------------------------
//                group state declaration macro
// -------------------------------------------------------------
#define REGISTER_ARMING_STATE(stateName, _ArmingState_)  \
    \
    static s32 handler_zone(ZoneEvent *event); \
    static s32 handler_button(ButtonEvent *event); \
    static s32 handler_arming(ArmingEvent *event); \
    static s32 handler_common_key(CommonKeyEvent *event); \
    static s32 handler_timer(InternalTimerEvent *event); \
    \
    GroupState stateName = {    \
                                _ArmingState_, \
                                #_ArmingState_, \
                                &handler_zone, \
                                &handler_button, \
                                &handler_arming, \
                                &handler_common_key, \
                                &handler_timer, \
                            };
// -------------------------------------------------------------


//
void Ar_GroupState_initStates(void);
void Ar_GroupState_setState(ArmingGroup *group, GroupStateIdent state);
bool Ar_GroupState_isDisarmed(ArmingGroup *group);
bool Ar_GroupState_isArmed(ArmingGroup *group);
void Ar_GroupState_initGroups(ArmingGroup_tbl *armingGroup_table);


#endif /* ARMING_STATES_H_ */
