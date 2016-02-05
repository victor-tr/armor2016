#include "armingstates.h"

#include "common/errors.h"
#include "common/debug_macro.h"


REGISTER_ARMING_STATE(state_armed_partial, STATE_ARMED_PARTIAL)


// -- handlers
s32 handler_zone(ZoneEvent *event)
{
    OUT_DEBUG_2("handler_zone() for %s\r\n", state_armed_partial.state_name);
    return RETURN_NO_ERRORS;
}

s32 handler_button(ButtonEvent *event)
{
    OUT_DEBUG_2("handler_button() for %s\r\n", state_armed_partial.state_name);
    return RETURN_NO_ERRORS;
}

s32 handler_arming(ArmingEvent *event)
{
    OUT_DEBUG_2("handler_arming() for %s\r\n", state_armed_partial.state_name);
    return RETURN_NO_ERRORS;
}

s32 handler_common_key(CommonKeyEvent *event)
{
    OUT_DEBUG_2("handler_common_key() for %s\r\n", state_armed_partial.state_name);
    return RETURN_NO_ERRORS;
}

s32 handler_timer(InternalTimerEvent *event)
{
    OUT_DEBUG_2("handler_timer() for %s\r\n", state_armed_partial.state_name);
    return RETURN_NO_ERRORS;
}
