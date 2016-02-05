#include "event_key.h"
#include "common/debug_macro.h"

#include "core/uart.h"
#include "common/errors.h"

#include "states/armingstates.h"

#include "service/humanize.h"


static inline bool isTouchMemoryKeyAlreadyReceived(ParentDevice *pParent, const u8 *key)
{
    bool already = TRUE;

    u8 *last_key = pParent->d.etr.last_received_keycode;

    for (u8 i = 0; i < TOUCHMEMORY_MAX_KEY_SIZE; ++i) {
        if (last_key[i] != key[i])
            already = FALSE;
        last_key[i] = key[i];
    }

    return already;
}


s32 process_mcu_touchmemory_key_msg(u8 *pkt)
{
    OUT_DEBUG_2("process_mcu_touchmemory_key_msg()\r\n");

    u8 * const key = &pkt[MCUPKT_BEGIN_DATABLOCK + 1];
    const PinTypeCode io_pin = (PinTypeCode)pkt[MCUPKT_BEGIN_DATABLOCK];

    if (MSG_GetValue == pkt[MCUPKT_COMMAND_CODE])
    {
        if (Pin_Output != io_pin)
            return ERR_BAD_PINTYPE_FOR_COMMAND;

        // --
        u16 etr_uin = pkt[MCUPKT_PARENTDEV_UIN_H] << 8 | pkt[MCUPKT_PARENTDEV_UIN_L];

        ParentDevice *pParent = getParentDeviceByUIN(etr_uin, OBJ_ETR);
        if (!pParent) {
            OUT_DEBUG_1("getParentDeviceByUIN(<%s #%d>) = FALSE\r\n",
                        getUnitTypeByCode(OBJ_ETR), etr_uin);
            return ERR_DB_RECORD_NOT_FOUND;
        }

//        if (isTouchMemoryKeyAlreadyReceived(pParent, key)) {
//            OUT_DEBUG_3("Received KEY already handled\r\n");
//            return RETURN_NO_ERRORS;
//        }


        OUT_DEBUG_3("KEY RECEIVED: %#x %#x %#x %#x %#x %#x %#x %#x\r\n",
                        key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);

        // -- don't process the key, only send it to configurator
        if (pParent->d.etr.bBlocked) {
            finishETRWatingKey(pParent);
            return sendLastReceivedTouchMemoryCode(etr_uin, key);
        }


        TouchmemoryCode *pCode = getTouchmemoryCodeByValue(key);
        if (!pCode) {
            OUT_DEBUG_1("getTouchmemoryCodeByValue(%#x %#x %#x %#x %#x %#x %#x %#x): key not found\r\n",
                        key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
            return ERR_DB_RECORD_NOT_FOUND; // unregistered key
        }

        OUT_DEBUG_3("ALIAS KEY RECEIVED: %s\r\n",
                        pCode->alias);


        if (ARMING_EVENT == pCode->action)
        {
            // -- check the group to ETR relation
            if (!isETRRelatedToGroup(pCode->group_id, &pParent->d.etr)) {
                OUT_DEBUG_3("The key is not valid for this ETR device\r\n");
                return RETURN_NO_ERRORS;
            }

            ArmingGroup *pGroup = getArmingGroupByID(pCode->group_id);
            if (!pGroup) {
                OUT_DEBUG_1("getArmingGroupByID(id=%d): group not found\r\n", pCode->group_id);
                return ERR_DB_RECORD_NOT_FOUND;
            }

            ArmingEvent ae;

            // -- key code info
            ae.key_info.key_type = OBJ_TOUCHMEMORY_CODE;
            ae.key_info.key = pCode;
            // --

            ae.emitter_type = (DbObjectCode)pkt[MCUPKT_SUBDEV_TYPE];
            ae.force = FALSE;   // won't be switched to arming if there are any error issures
            ae.p_group = pGroup;
            ae.p_etr=pParent;
            Ql_GetLocalTime(&ae.time);
            ae.p_current_arming_state = pGroup ? pGroup->p_group_state : NULL;

            // -- invoke Handler
            if (ae.p_current_arming_state) {
                s32 ret = ae.p_current_arming_state->handler_arming(&ae);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("%s: handler_arming() = %d error.\r\n",
                                ae.p_current_arming_state->state_name, ret);
                    return ret;
                }
            }
        }
        else if (COMMON_KEY_EVENT == pCode->action)
        {
            ArmingGroup *pGroup = getArmingGroupByID(pCode->group_id);
            if (pGroup) {
                if (!isETRRelatedToGroup(pCode->group_id, &pParent->d.etr)) {
                    OUT_DEBUG_3("The key is not valid for this ETR device\r\n");
                    return RETURN_NO_ERRORS;
                }
            }

            CommonKeyEvent e;

            // -- key code info
            e.key_info.key_type = OBJ_TOUCHMEMORY_CODE;
            e.key_info.key = pCode;
            // --

            e.emitter_type = (DbObjectCode)pkt[MCUPKT_SUBDEV_TYPE];
            e.force = FALSE;   // won't be processed if there are any error issures

            Ql_GetLocalTime(&e.time);
            e.p_group = pGroup;

            e.p_current_arming_state = pGroup ? pGroup->p_group_state : NULL;

            // -- invoke group related Handler
            if (e.p_current_arming_state) {
                s32 ret = e.p_current_arming_state->handler_common_key(&e);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("%s: handler_common_key() = %d error.\r\n",
                                e.p_current_arming_state->state_name, ret);
                    return ret;
                }
            }
            // -- else invoke common key Handler
            else {
                s32 ret = global_handler_common_key(&e);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("global_handler_common_key() = %d error.\r\n", ret);
                    return ret;
                }
            }
        }
    }

    return RETURN_NO_ERRORS;
}


s32 global_handler_common_key(CommonKeyEvent *event)
{
    OUT_DEBUG_2("global_handler_common_key()\r\n");

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

    s32 ret = processReactions(pEvt, NULL);   // WARNING: magic number - only one event is possible now
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("processReactions() = %d error.\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}
