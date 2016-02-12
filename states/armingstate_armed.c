#include "armingstates.h"

#include "core/system.h"
#include "core/mcu_tx_buffer.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "../db/lexicon.h"

#include "pult/pult_tx_buffer.h"


REGISTER_ARMING_STATE(state_armed, STATE_ARMED)


// -- handlers
s32 handler_zone(ZoneEvent *event)
{
    OUT_DEBUG_2("handler_zone() for %s\r\n", state_armed.state_name);
    CommonSettings *cs = getCommonSettings(NULL);
    Zone *pZone = event->p_zone;
    ArmingGroup *pGroup = getArmingGroupByID(pZone->group_id);

    // -- classify the event
    if (ZONE_EVENT_NORMAL == event->zone_event_type ||
            ZONE_EVENT_DISABLED == event->zone_event_type) {
        Ar_System_setZoneAlarmedFlag(pZone, FALSE);
        batchSetETRLedIndicatorsLedState(pZone, UnitStateOff);


//        if (ZONE_ENTRY_DELAY == pZone->zone_type) {

//            if (pGroup->disarming_zoneDelay_counter > 0 &&
//                                   pGroup->disarming_zoneDelay_counter < cs->entry_delay_sec)
//            {
//                ParentDevice *pPrev_ETR = getParentDeviceByID(pGroup->change_status_ETR->id, OBJ_ETR);
//                if(pPrev_ETR){
//                    ETR *pEtr = &pPrev_ETR->d.etr;
//                    OUT_DEBUG_7("ENTRY zone normal\r\n");
//                    OUT_DEBUG_7("enter counter=%d \r\n",
//                                pGroup->change_status_ETR->d.etr.buzzer.counter);
//                    if(pPrev_ETR->d.etr.buzzer.counter>0)
//                        --pPrev_ETR->d.etr.buzzer.counter;
//                    OUT_DEBUG_7("enter counter=%d \r\n",
//                                pGroup->change_status_ETR->d.etr.buzzer.counter);
//                }
//            }

//        }



    } else {
        Ar_System_setZoneAlarmedFlag(pZone, TRUE);
        batchSetETRLedIndicatorsLedState(pZone, UnitStateOn);
    }


    // -- perform predefined operations

    // -- process the reactions list
    processReactions(pZone->events[event->zone_event_type], &state_armed);


 OUT_DEBUG_7("ENTRY() 1\r\n");
    // -- check that the zone is in delay or not
    if (isArmingDelayActive(event->p_zone)) {
        OUT_DEBUG_3("Zone %d is in delayed state now\r\n", event->p_zone->humanized_id);
        return RETURN_NO_ERRORS;
    }
 OUT_DEBUG_7("ENTRY() 2\r\n");
    if (isDisarmingDelayActive(event->p_zone)) {
        OUT_DEBUG_3("Zone %d is in delayed state now\r\n", event->p_zone->humanized_id);
        return RETURN_NO_ERRORS;
    }
 OUT_DEBUG_7("ENTRY() 3\r\n");

    if (ZONE_ENTRY_DELAY == pZone->zone_type) {
        OUT_DEBUG_7("ENTRY() \r\n");
        OUT_DEBUG_7("AG =%d \r\n",pGroup->id);
        Ar_System_startEtrBuzzer(pGroup, 6, 4, 0, TRUE);
        Ar_System_setPrevETRLed(pGroup, UnitStateMultivibrator);
    }


    if (ZONE_ENTRY_DELAY != pZone->zone_type)
    {
        if (ZONE_WALK_THROUGH != pZone->zone_type )
        {
        Ar_System_setBellState(UnitStateMultivibrator);
        }
        else if(pGroup->disarming_zoneDelay_counter == 0  && pGroup->arming_zoneDelay_counter == 0)
        {
            Ar_System_setBellState(UnitStateMultivibrator);
        }
    }



    OUT_DEBUG_3("Zone counter= %d \r\n", pGroup->arming_zoneDelay_counter);

    // -- create message for the pult
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
            } else {                                        // all other zones
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

    return RETURN_NO_ERRORS;
}

s32 handler_button(ButtonEvent *event)
{
    OUT_DEBUG_2("handler_button() for %s\r\n", state_armed.state_name);

//    Button *pButton = event->p_button;

    // -- process the reactions list
//    s32 ret = processReactions(pButton->events[event->button_event_type]);
//    if (ret < RETURN_NO_ERRORS) {
//        OUT_DEBUG_1("processReactions() = %d error.\r\n", ret);
//        return ret;
//    }

    return RETURN_NO_ERRORS;
}

s32 handler_arming(ArmingEvent *event)
{
    OUT_DEBUG_2("handler_arming() for %s\r\n", state_armed.state_name);
    ParentDevice *pParent=event->p_etr;

    // -- stop last and start new ETR
    ArmingGroup *pGroup = getArmingGroupByID(event->p_group->id);
    Ar_System_setPrevETRLed(pGroup, UnitStateOff);
    OUT_DEBUG_7("KEY off() \r\n");
    OUT_DEBUG_7("AG =%d \r\n",pGroup->id);
    Ar_System_stopPrevETRBuzzer(pGroup);

    stopBothDelays(event->p_group);
    Ar_System_startEtrBuzzer(pGroup, 9, 1, 1, FALSE);

    Ar_GroupState_setState(event->p_group, STATE_DISARMED);

    //==================
    Ar_System_setBellState(UnitStateOff);
    Ar_System_setBellBeep();

    s32 retTable = Ar_System_refreshGroupState();
    if (retTable < RETURN_NO_ERRORS)
          OUT_DEBUG_1("Error %d! Ar_System_refreshGroupState()\r\n", retTable);
    Ar_System_showGroupState();


    //===================

    PultMessage msg = {0};

    if (OBJ_TOUCHMEMORY_CODE == event->key_info.key_type) {
        TouchmemoryCode *keycode = event->key_info.key;
        msg.identifier = keycode->id;
        OUT_DEBUG_3("Disarmed by key: \"%s\"\r\n", keycode->alias);
    } else if (OBJ_KEYBOARD_CODE == event->key_info.key_type) {
        KeyboardCode *keycode = event->key_info.key;
        msg.identifier = keycode->id;
        OUT_DEBUG_3("Disarmed by key: \"%s\"\r\n", keycode->alias);
    }

    if (!msg.identifier)
        return ERR_INVALID_PARAMETER;

    msg.group_id = event->p_group->id;

    msg.msg_code = PMC_OpenClose;
    msg.msg_qualifier = PMQ_NewMsgOrSetDisarmed;

    s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 handler_common_key(CommonKeyEvent *event)
{
    OUT_DEBUG_2("handler_common_key() for %s\r\n", state_armed.state_name);

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

    s32 ret = processReactions(pEvt, &state_armed);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("processReactions() = %d error.\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 handler_timer(InternalTimerEvent *event)
{
    OUT_DEBUG_2("handler_timer() for %s\r\n", state_armed.state_name);
    return RETURN_NO_ERRORS;
}
