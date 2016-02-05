#include "armingstates.h"
#include "common/debug_macro.h"
#include "events/event_group.h"
#include "events/events.h"


// -- states
extern GroupState state_disarmed;
extern GroupState state_disarmed_forced;
extern GroupState state_disarmed_ing;
extern GroupState state_armed;
extern GroupState state_armed_partial;
extern GroupState state_armed_ing;
extern GroupState state_configuring;


static GroupState *group_states[STATE_MAX] = {0};


void Ar_GroupState_initStates(void)
{
    group_states[STATE_DISARMED] = &state_disarmed;
    group_states[STATE_DISARMED_FORCED] = &state_disarmed_forced;
    group_states[STATE_DISARMED_ING] = &state_disarmed_ing;
    group_states[STATE_ARMED] = &state_armed;
    group_states[STATE_ARMED_PARTIAL] = &state_armed_partial;
    group_states[STATE_ARMED_ING] = &state_armed_ing;
    group_states[STATE_CONFIGURING] = &state_configuring;
}

void Ar_GroupState_setState(ArmingGroup *group, GroupStateIdent state)
{
    OUT_DEBUG_2("Ar_GroupState_setState(id: %d, \"%s\")\r\n", group->id, group_states[state]->state_name);
    group->p_prev_group_state =  group->p_group_state;
    group->p_group_state = group_states[state];

    // start reaction event
    if(state == STATE_ARMED || state == STATE_DISARMED )
    {
        SystemEventType sysET = GROUP_EVENT_STATE_CHANGED_DISARMED_TO_ARMED;
        switch (state)
        {
        case STATE_ARMED:
            sysET = GROUP_EVENT_STATE_CHANGED_DISARMED_TO_ARMED;
            break;
        case STATE_DISARMED:
            sysET = GROUP_EVENT_STATE_CHANGED_ARMED_TO_DISARMED;
            break;
        }

        s32 ret = common_handler_group(sysET);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("common_handler_group() = %d error\r\n", ret);
        }
    }
}

bool Ar_GroupState_isDisarmed(ArmingGroup *group)
{
    return group->p_group_state == group_states[STATE_DISARMED];
}

bool Ar_GroupState_isArmed(ArmingGroup *group)
{
    return group->p_group_state == group_states[STATE_ARMED];
}

void Ar_GroupState_initGroups(ArmingGroup_tbl *armingGroup_table)
{
    for (u8 i = 0; i < armingGroup_table->size; ++i) {

        armingGroup_table->t[i].p_prev_group_state =  group_states[STATE_DISARMED];
        armingGroup_table->t[i].p_group_state = group_states[STATE_DISARMED];
	}
}

