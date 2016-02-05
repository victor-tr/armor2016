#include "armingstates.h"
#include "pult/pult_tx_buffer.h"
#include "common/errors.h"
#include "common/debug_macro.h"
#include "pult/connection.h"
#include "core/system.h"


REGISTER_ARMING_STATE(state_configuring, STATE_CONFIGURING)


// -- handlers
s32 handler_zone(ZoneEvent *event)
{
    OUT_DEBUG_2("handler_zone() for %s\r\n", state_configuring.state_name);
    return RETURN_NO_ERRORS;
}

s32 handler_button(ButtonEvent *event)
{
    OUT_DEBUG_2("handler_button() for %s\r\n", state_configuring.state_name);
    return RETURN_NO_ERRORS;
}

s32 handler_arming(ArmingEvent *even)
{
    OUT_DEBUG_2("handler_arming() for %s\r\n", state_configuring.state_name);
    OUT_DEBUG_3("The system can not be armed from this state by user\r\n");
    return RETURN_NO_ERRORS;
}

s32 handler_common_key(CommonKeyEvent *event)
{
    OUT_DEBUG_2("handler_common_key() for %s\r\n", state_configuring.state_name);

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

    s32 ret = processReactions(pEvt, &state_configuring);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("processReactions() = %d error.\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 handler_timer(InternalTimerEvent *event)
{
    OUT_DEBUG_2("handler_timer() for %s\r\n", state_configuring.state_name);
    return RETURN_NO_ERRORS;
}
