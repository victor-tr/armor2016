#ifndef HUMANIZE_H
#define HUMANIZE_H

#include "ql_type.h"


#define CASE_RETURN_NAME(value) \
    case value:  \
    {   \
        static const char _##value##_text[] = #value; \
        return _##value##_text; \
    }

#define CASE_RETURN_CUSTOM_NAME(value, namestr) \
    case value:  \
    {   \
        static const char _##value##_text[] = namestr; \
        return _##value##_text; \
    }

#define DEFAULT_RETURN_CODE(code) \
    default: \
    { \
        static char _buf[10] = {0}; \
        Ql_memset(_buf, 0, sizeof(_buf)); \
        Ql_sprintf(_buf, "%d", code); \
        return _buf; \
    }

#define DEFAULT_RETURN_STR(str) \
    default: { static const char *s = str; return s; }



const char *getChannelType_humanize(void);
const char *getChannelState_humanize(void);
const char *getActiveSimcard_humanize(void);
const char *getCodec_humanize(void);

const char *getUnitTypeByCode(u8 code);


#endif // HUMANIZE_H
