#include "event_group.h"
#include "common/debug_macro.h"

#include "common/errors.h"

#include "service/humanize.h"


s32 common_handler_group(SystemEventType group_event)
{

    OUT_DEBUG_2("common_handler_group(). SystemEventType = %d\r\n", group_event);
    return RETURN_NO_ERRORS;
}
