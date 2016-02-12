#include "armingstates.h"

#include "core/system.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "pult/pult_tx_buffer.h"


REGISTER_ARMING_STATE(state_disarmed, STATE_DISARMED)


// -- handlers
s32 handler_zone(ZoneEvent *event)
{
    OUT_DEBUG_2("handler_zone() for %s\r\n", state_disarmed.state_name);

    Zone *pZone = event->p_zone;

    // -- classify the event
    if (ZONE_EVENT_NORMAL == event->zone_event_type ||
            ZONE_EVENT_DISABLED == event->zone_event_type) {
        Ar_System_setZoneAlarmedFlag(pZone, FALSE);
        batchSetETRLedIndicatorsLedState(pZone, UnitStateOff);
    } else {
        Ar_System_setZoneAlarmedFlag(pZone, TRUE);
        batchSetETRLedIndicatorsLedState(pZone, UnitStateOn);
    }


    // -- perform predefined operations


    // -- process the reactions list
    processReactions(pZone->events[event->zone_event_type], &state_disarmed);

    // -- create message for the pult
    if (ZONE_24_HOUR == pZone->zone_type ||
            ZONE_PANIC == pZone->zone_type ||
            ZONE_FIRE == pZone->zone_type)
    {
        PultMessage msg;
        if (1 /*pZone->bus_type == ZBT_SerialLine*/) {

            msg.identifier = pZone->humanized_id;
            msg.group_id = pZone->group_id;

            switch (event->zone_event_type) {
            case ZONE_EVENT_NORMAL:
                msg.msg_code = PMC_Burglary;
                msg.msg_qualifier = PMQ_RestoralOrSetArmed;
                break;
            case ZONE_EVENT_BREAK:
                if (ZONE_ENTRY_DELAY == pZone->zone_type) {     // e.g. open door
                    startDisarmingDelay(event->p_group, event->p_zone);
                    msg.msg_code = PMC_UserOnPremise;
                }
                else if (ZONE_PANIC == pZone->zone_type) {
                    msg.msg_code = PMC_Silent;
                }
                else {        // all other zones
                    msg.msg_code = PMC_Burglary;
                }
                msg.msg_qualifier = PMQ_NewMsgOrSetDisarmed;
                break;
            case ZONE_EVENT_SHORT:
                msg.msg_code = PMC_PollingLoop_Short;
                msg.msg_qualifier = PMQ_NewMsgOrSetDisarmed;
                break;
            case ZONE_EVENT_FITTING:
                msg.msg_code = PMC_PollingLoop_Fitting;
                msg.msg_qualifier = PMQ_NewMsgOrSetDisarmed;
                break;
            case ZONE_EVENT_DISABLED:
                OUT_DEBUG_3("Zone %d has been disabled. The message will not send.", msg.identifier);
                return RETURN_NO_ERRORS;
            default:
                msg.msg_code = (PultMessageCode)0;
                msg.msg_qualifier = (PultMessageQualifier)0;
            }

        } else if (0 /*pZone->bus_type == ZBT_ParallelLine*/) {
            // _TODO: describe the same for parallel protection loop
        }

        s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
            return ret;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 handler_button(ButtonEvent *event)
{
    OUT_DEBUG_2("handler_button() for %s\r\n", state_disarmed.state_name);
OUT_DEBUG_8("DO NOTHING\r\n");
    return RETURN_NO_ERRORS;
}

s32 handler_arming(ArmingEvent *event)
{
    OUT_DEBUG_2("handler_arming() for %s\r\n", state_disarmed.state_name);

    /* don't process arming event if any of alarm events is presented */
//    if (!event->force && isGroupAlarmed(event->p_group))
//        return RETURN_NO_ERRORS;
    ArmingGroup *pGroup = getArmingGroupByID(event->p_group->id);
    ParentDevice *pParent=event->p_etr;
    if (!pParent) {
        OUT_DEBUG_2("handler_arming()parent device not found \r\n");
        return ERR_DB_RECORD_NOT_FOUND;
    }


    // test zone
    bool bAllEnabledZonesIsNormal = Ar_System_isZonesInGroupNormal(pGroup);

    pGroup->change_status_ETR=pParent;

    if (!bAllEnabledZonesIsNormal)
    {

         Ar_System_startEtrBuzzer(pGroup, 9, 1, 1, FALSE);
//         Ar_System_startEtrArmedLED(pGroup, 5, 5, 2);
        //Ar_System_startEtrBuzzer(pGroup, 5, 5, 2, FALSE);

        return RETURN_NO_ERRORS;
    }

    // save last ETR


    startArmingDelay(event->p_group);
    Ar_GroupState_setState(event->p_group, STATE_ARMED);
    Ar_System_setBellBeep();


    s32 retTable = Ar_System_refreshGroupState();
    if (retTable < RETURN_NO_ERRORS)
          OUT_DEBUG_1("Error %d! Ar_System_refreshGroupState()\r\n", retTable);
    Ar_System_showGroupState();


    PultMessage msg = {0};

    if (OBJ_TOUCHMEMORY_CODE == event->key_info.key_type) {
        TouchmemoryCode *keycode = event->key_info.key;
        msg.identifier = keycode->id;
        OUT_DEBUG_3("Armed by key: \"%s\"\r\n", keycode->alias);
    } else if (OBJ_KEYBOARD_CODE == event->key_info.key_type) {
        KeyboardCode *keycode = event->key_info.key;
        msg.identifier = keycode->id;
        OUT_DEBUG_3("Armed by key: \"%s\"\r\n", keycode->alias);
    }

    if (!msg.identifier)
        return ERR_INVALID_PARAMETER;

    msg.group_id = event->p_group->id;

    msg.msg_code = PMC_OpenClose;
    msg.msg_qualifier = PMQ_RestoralOrSetArmed;

    s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 handler_common_key(CommonKeyEvent *event)
{
    OUT_DEBUG_2("handler_common_key() for %s\r\n", state_disarmed.state_name);

    Event *pEvt = NULL;

    if (OBJ_TOUCHMEMORY_CODE == event->key_info.key_type)
    {
        TouchmemoryCode *pTMCode = event->key_info.key;
        pEvt = pTMCode->events[0];
    }
    else if (OBJ_KEYBOARD_CODE == event->key_info.key_type)
    {
        KeyboardCode *pKbdCode = event->key_info.key;
        pEvt = pKbdCode->events[0];
    }

    s32 ret = processReactions(pEvt, &state_disarmed);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("processReactions() = %d error.\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 handler_timer(InternalTimerEvent *event)
{
    OUT_DEBUG_2("handler_timer() for %s\r\n", state_disarmed.state_name);
    return RETURN_NO_ERRORS;
}
