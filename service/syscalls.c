#include "common/debug_macro.h"
#if defined(__GNUC__) && defined(FORCE_PREFER_DEBUG_OUT_FUNC)
#include <sys/types.h>
#include "core/system.h"
#include "config/custom_heap_cfg.h"


caddr_t _sbrk(int incr) {
    return NULL;
}


#endif  // defined(__GNUC__) && defined(FORCE_PREFER_DEBUG_OUT_FUNC)
