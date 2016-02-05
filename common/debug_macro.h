/*
 * debug_macro.h
 *
 * Created: 22.10.2012 16:11:03
 *  Author: user
 */


#ifndef DEBUG_MACRO_H_
#define DEBUG_MACRO_H_

#include "ql_trace.h"
#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_error.h"

#include "../../custom/common/defines.h"


#define FORCE_PREFER_DEBUG_OUT_FUNC
#define USE_DEBUG_OUT_MUTEX


#define _PREFIX_LEN 7
#define _DEBUG_UART_PORT UART_PORT2

#define _MIN_REMAIN         30
#define _TEMPBUF_SIZE       1024*4+1
#define _TEMP_OUT_BUF_SIZE       512


typedef enum {
    DBS_Ready,
    DBS_Busy,
    DBS_ProcessLater
} DbgBufStatus;

typedef struct {
    u8   data[DEBUG_BUF_SIZE];
    u8   *head;
    u8   *tail;
    u8   tempbuf[_TEMPBUF_SIZE];  // single message can't be longer than the size of this buffer
    DbgBufStatus status;
    u32  mutex;

    u8 uOverFlag;
    s32 FreeSpace;
    s32 BusySpace;
    u8  *lostTextAdr;
    u8  temp_out_buff[_TEMP_OUT_BUF_SIZE];

} DebugTextBuffer;

extern DebugTextBuffer textBuf;

u8 debugLevel(void);


#ifdef USE_DEBUG_OUT_MUTEX
#define REGISTER_OUT_DEBUG_MUTEX   textBuf.mutex = Ql_OS_CreateMutex("out_debug_mutex");
#define _LOCK_OUT_DEBUG_MUTEX      Ql_OS_TakeMutex(textBuf.mutex);
#define _RELEASE_OUT_DEBUG_MUTEX   Ql_OS_GiveMutex(textBuf.mutex);
#else
#define REGISTER_OUT_DEBUG_MUTEX
#define _LOCK_OUT_DEBUG_MUTEX
#define _RELEASE_OUT_DEBUG_MUTEX
#endif


u8 *getHead();
void reverseHead(s32 reverse_offset);
u8 *getTail();
void Rasklad(void);

#define _RESET_DBG_BUFFER   \
    Ql_memset(textBuf.data, 0, DEBUG_BUF_SIZE);  \
    textBuf.head = textBuf.data;    \
    textBuf.tail = textBuf.data;    \
    textBuf.status = DBS_Ready;     \
    textBuf.FreeSpace = DEBUG_BUF_SIZE; \
    textBuf.BusySpace = 0;  \
    textBuf.uOverFlag = 0;  \
    textBuf.lostTextAdr = NULL;



#define _WRITE_TO_DBG_UART_CYCL \
{ \
    textBuf.status = DBS_Ready;  \
    s32 real_size_temp_out_buff = 0;    \
    \
    for(u16 i = 0; i <  _TEMP_OUT_BUF_SIZE; ++i)    \
    {   \
            u8 *localHead = getHead();  \
            if(NULL == localHead)   \
            {   \
                break;  \
            }   \
            else    \
            {   \
                *(textBuf.temp_out_buff+i) = *localHead;  \
            }   \
            real_size_temp_out_buff++;  \
    }  \
    if(textBuf.BusySpace>0) \
    {   \
        textBuf.status = DBS_ProcessLater; \
    }   \
    \
    if(real_size_temp_out_buff > 0) \
    {   \
        s32 sent_len = Ql_UART_Write(_DEBUG_UART_PORT, textBuf.temp_out_buff, real_size_temp_out_buff); \
        if (sent_len < real_size_temp_out_buff) {    \
            reverseHead(real_size_temp_out_buff - sent_len);  \
            textBuf.status = DBS_Busy;  \
        } \
    }   \
}

//                Ql_UART_Write(_DEBUG_UART_PORT, "TEST\r\n", 8);    \

#define _DEBUG_CYCL_ROUTINE_BEGIN(debug_level, prefix_text)   \
    do { \
        if (debug_level <= debugLevel()) { \
            _LOCK_OUT_DEBUG_MUTEX \
                for(u16 i = 0; i < Ql_strlen((char *)prefix_text); ++i)    \
                {   \
                    u8 *localTail = getTail();  \
                    if(NULL == localTail)   \
                    {   \
                        break;  \
                    }   \
                    else    \
                    {   \
                        *localTail = *(prefix_text + i);   \
                    }   \
                }   \
\
            Ql_memset(textBuf.tempbuf, 0, sizeof(textBuf.tempbuf)); \
            {




#define _DEBUG_CYCL_ROUTINE_END   \
            }   \
            for(u16 i = 0; i <  Ql_strlen((char *)textBuf.tempbuf)   ; ++i)    \
            {   \
                u8 *localTail = getTail();  \
                if(NULL == localTail)   \
                {   \
                    break;  \
                }   \
                else    \
                {   \
                    *localTail = *(textBuf.tempbuf+i);  \
                }   \
            }   \
  \
            if (DBS_Busy != textBuf.status) { \
                _WRITE_TO_DBG_UART_CYCL  \
            }   \
            _RELEASE_OUT_DEBUG_MUTEX \
        }   \
    } while (0);


#define CONTINUE_OUT_DEBUG_CYCL    \
    do {    \
        _WRITE_TO_DBG_UART_CYCL \
    } while (0);




#if !defined(__GNUC__) || defined(FORCE_PREFER_DEBUG_OUT_FUNC)
extern void out_debug(u8 dbgLevel, const char *prefix, const char *fmt, ...);
extern void out_debug_unprefixed(u8 dbgLevel, const char *fmt, ...);


// returned codes tracking
#define OUT_DEBUG_1(...)    out_debug(1, "[ERR ] ", __VA_ARGS__);

// enter to a function tracking
#define OUT_DEBUG_2(...)    out_debug(2, "[FUNC] ", __VA_ARGS__);

// arbitrary events tracking
#define OUT_DEBUG_3(...)    out_debug(3, "[INFO] ", __VA_ARGS__);

// -- GPRS tracking
#define OUT_DEBUG_GPRS(...)     out_debug(4, "[GPRS] ", __VA_ARGS__);

// -- AT commands tracking
#define OUT_DEBUG_ATCMD(...)    out_debug(5, "[ AT ] ", __VA_ARGS__);

// -- Unprefixed line
#define OUT_UNPREFIXED_LINE(...)    out_debug_unprefixed(1, __VA_ARGS__);

#define OUT_DEBUG_6(...)    out_debug(6, "[DBG6] ", __VA_ARGS__);

#define OUT_DEBUG_7(...)    out_debug(7, "[DBG7] ", __VA_ARGS__);

// -- replacement for Ql_DebugTrace()
#define OUT_DEBUG_8(...)    out_debug(8, "[DBG8] ", __VA_ARGS__);


#else // => used by GNU GCC compiler
// returned codes tracking
#define OUT_DEBUG_1(...)    /*out_debug(1, "[ERR ] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(1, "[ERR ] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

// enter to a function tracking
#define OUT_DEBUG_2(...)    /*out_debug(2, "[FUNC] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(2, "[FUNC] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

// arbitrary events tracking
#define OUT_DEBUG_3(...)    /*out_debug(3, "[INFO] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(3, "[INFO] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

// -- GPRS tracking
#define OUT_DEBUG_GPRS(...)     /*out_debug(4, "[GPRS] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(4, "[GPRS] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

// -- AT commands tracking
#define OUT_DEBUG_ATCMD(...)    /*out_debug(5, "[ AT ] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(5, "[ AT ] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

// -- Unprefixed line
#define OUT_UNPREFIXED_LINE(...)    /*out_debug_unprefixed(1, __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(1, "")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

#define OUT_DEBUG_6(...)    /*out_debug(6, "[DBG6] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(6, "[DBG6] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

#define OUT_DEBUG_7(...)    /*out_debug(7, "[DBG7] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(7, "[DBG7] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }


// -- replacement for Ql_DebugTrace()
#define OUT_DEBUG_8(...)    /*out_debug(8, "[DBG8] ", __VA_ARGS__);*/\
    {   \
        _DEBUG_ROUTINE_BEGIN(8, "[DBG8] ")   \
        Ql_sprintf((char*)textBuf.tempbuf, __VA_ARGS__);   \
        _DEBUG_ROUTINE_END  \
    }

#endif // !defined(__GNUC__ || ) || defined(FORCE_PREFER_DEBUG_OUT_FUNC)


#endif /* DEBUG_MACRO_H_ */
