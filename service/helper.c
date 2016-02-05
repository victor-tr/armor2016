#include "helper.h"
#include "common/debug_macro.h"
#include "common/defines.h"

#define SIZE_BUFF CODEC_TX_BUFFER_SIZE*3

void Ar_Helper_debugOutDataPacket(u8 *data, u16 len)
{

    char datagram_ascii[SIZE_BUFF + 1] = {0};
    char *iter = datagram_ascii;
    for (u16 i = 0; i < len; ++i) {
        if (iter + 4 > datagram_ascii + SIZE_BUFF)
            break;
        char str[4] = {0};
        if(data[i]>0xF)
        {
            Ql_sprintf(str, "%X", data[i]);
        }
        else
        {
            Ql_sprintf(str, "0%X", data[i]);
        }
        Ql_strcpy(iter, str);
        iter += Ql_strlen(str);
        if (i + 1 < len)
            *iter++ = ' ';
    }
    OUT_DEBUG_7("DATA <len=%d> :hex: [%s]\r\n", len, datagram_ascii);
}
