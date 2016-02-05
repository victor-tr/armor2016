#include "debug_macro.h"
#if !defined(__GNUC__)  // used by RVCT
#include <windows/stdarg.h>
#include <windows/stdio.h>
#endif
#if defined(__GNUC__) && defined(FORCE_PREFER_DEBUG_OUT_FUNC) // used by GCC when FORCE_PREFER_DEBUG_OUT_FUNC
#include <stdarg.h>
#include <stdio.h>
#endif


DebugTextBuffer textBuf;


void reverseHead(s32 reverse_offset)
{

    for(s32 i = reverse_offset; i > 0; --i)
    {
        if(textBuf.data == textBuf.head)
        {
            textBuf.head = textBuf.data + DEBUG_BUF_SIZE - 1;
        }
        else
        {
            textBuf.head--;
        }
        textBuf.FreeSpace--;
        textBuf.BusySpace++;
    }
    Rasklad();
}

u8 *getHead()
{

    // check over
    if(textBuf.uOverFlag > 0 && textBuf.lostTextAdr == textBuf.head)
    {
        Ql_strcpy((char *)textBuf.temp_out_buff, "[@OVF] ");
        Ql_UART_Write(_DEBUG_UART_PORT, textBuf.temp_out_buff, _PREFIX_LEN);

        _RESET_DBG_BUFFER

        return NULL;
    }

    if(textBuf.tail == textBuf.head )
    {
        return NULL;
    }
    else
    {

            textBuf.FreeSpace++;
            textBuf.BusySpace--;
            if((textBuf.data + DEBUG_BUF_SIZE -1) == textBuf.head)
            {
                textBuf.head = textBuf.data;
                return (textBuf.data + DEBUG_BUF_SIZE - 1);
            }
            else
            {
                textBuf.head++;
                return (textBuf.head - 1);
            }

        return textBuf.head;
    }
}

u8 *getTail()
{

    if(textBuf.FreeSpace>0)
    {
        textBuf.FreeSpace--;
        textBuf.BusySpace++;
        if((textBuf.data + DEBUG_BUF_SIZE - 1) == textBuf.tail)
        {
            textBuf.tail = textBuf.data;
            return (textBuf.data + DEBUG_BUF_SIZE - 1);
        }
        else
        {
            textBuf.tail++;
            return (textBuf.tail - 1);
        }

    }
    else
    {
        if(textBuf.uOverFlag == 0)
        {
            textBuf.lostTextAdr = textBuf.tail;
        }
        textBuf.uOverFlag++;
        return NULL;
    }
}


void Rasklad(void)
{
        char cmd[20] = {0};

        Ql_sprintf(cmd, "STRT=%p \r\n", textBuf.data);
        Ql_UART_Write(_DEBUG_UART_PORT, cmd, 20);

        Ql_sprintf(cmd, "HEAD=%p \r\n", textBuf.head);
        Ql_UART_Write(_DEBUG_UART_PORT, cmd, 20);

        Ql_sprintf(cmd, "TAIL=%p \r\n", textBuf.tail);
        Ql_UART_Write(_DEBUG_UART_PORT, cmd, 20);

        Ql_sprintf(cmd, "FNSH=%p \r\n", textBuf.data + DEBUG_BUF_SIZE);
        Ql_UART_Write(_DEBUG_UART_PORT, cmd, 20);

        Ql_sprintf(cmd, "FREE=%d \r\n", textBuf.FreeSpace);
        Ql_UART_Write(_DEBUG_UART_PORT, cmd, 20);

        Ql_sprintf(cmd, "BUSY=%d \r\n", textBuf.BusySpace);
        Ql_UART_Write(_DEBUG_UART_PORT, cmd, 20);

}

#if !defined(__GNUC__) || defined(FORCE_PREFER_DEBUG_OUT_FUNC)
void out_debug(u8 dbgLevel, const char *prefix, const char *fmt, ...)
{
    _DEBUG_CYCL_ROUTINE_BEGIN (dbgLevel, prefix)
    va_list args;
    va_start(args, fmt);
    vsprintf((char*)textBuf.tempbuf, fmt, args);
    va_end(args);
    _DEBUG_CYCL_ROUTINE_END
}

void out_debug_unprefixed(u8 dbgLevel, const char *fmt, ...)
{
    _DEBUG_CYCL_ROUTINE_BEGIN(dbgLevel, "") // prefix is empty string
//    textBuf.tail -= _PREFIX_LEN;
//    remain += _PREFIX_LEN;
    va_list args;
    va_start(args, fmt);
    vsprintf((char*)textBuf.tempbuf, fmt, args);
    va_end(args);
    _DEBUG_CYCL_ROUTINE_END
}
#endif




/* template of the implementation of function with var args (like prinf() in std lib */
//void out_debug_1(const char *fmt, ...)
//{
//    va_list args;
//    const char *p = NULL;
//    static char buf[_TEMPBUF_SIZE] = {0};
//    char *q = buf;
//    int i = 0;
//    char *s = 0;

//    va_start(args, fmt);

//    for (p = fmt; p != '\0'; ++p) {
//        if (*p != '%') {
//            *q = *p;   // print character
//        }
//        else {
//            switch (*++p) {
//            case 'c':
//                i = va_arg(args, int);
//                break;
//            case 'd':
//                i = va_arg(args, int);
//                break;
//            case 's':
//                s = va_arg(args, char *);
//                break;
//            case 'x':
//                i = va_arg(args, int);
//                break;
//            case '%':
//                *q = '%';
//                break;
//            }
//        }
//        ++q;
//    }

//    va_end(args);
//    // print 'buf' to uart port here
//}
