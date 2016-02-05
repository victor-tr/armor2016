#include "pult_tx_buffer.h"
#include "core/system.h"


static void pultTxBuffer_enqueue_callback(void)
{
    // protection: free up space for new msg if the buffer is filled up
    if (pultTxBuffer.size() > PULT_FIFO_CAPACITY) {
        OUT_DEBUG_3("pultTxBuffer buffer overflow\r\n");
        pultTxBuffer.removeFirstN(2);
        // TODO: handle the buffer overflow event and send the msg
    }

    OUT_DEBUG_2("pultTxBuffer.status() = %d\r\n", pultTxBuffer.status());
    if (QueueStatus_ReadyToProcessing == pultTxBuffer.status())
    {
        OUT_DEBUG_2("pultTxBuffer_enqueue_callback()\r\n");
        pultTxBuffer.setStatus(QueueStatus_InProcessing);
        fa_SendMessageToPult();
    }


}

static void pultTxBuffer_msg_cleanup_callback(PultMessage *msg)
{ if (msg && msg->bComplex && msg->complex_msg_part) Ql_MEM_Free(msg->complex_msg_part); }


REGISTER_FIFO_C(PultMessage, pultTxBuffer, &pultTxBuffer_enqueue_callback, &pultTxBuffer_msg_cleanup_callback)


s32 reportToPult(const u16 identifier,
                 const u16 group_id,
                 const PultMessageCode msg_code,
                 const PultMessageQualifier msg_qualifier)
{
    if (Ar_System_checkFlag(SysF_DeviceConfiguring))
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("reportToPult(q=%d, xyz=%d, group=%d, ident=%d)\r\n",
                msg_qualifier, msg_code, group_id, identifier);

    PultMessage m = {0};

    m.bComplex = FALSE;
    m.complex_msg_part = NULL;
    m.part_len = 0;
    m.frameNo = 0;
    m.frameTotal = 0;

    m.identifier = identifier;
    m.group_id = group_id;
    m.msg_code = msg_code;
    m.msg_qualifier = msg_qualifier;
    m.msg_type      = PMT_Normal;
    m.account = 0;

    s32 ret = pultTxBuffer.enqueue(&m);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("pultTxBuffer::enqueue() = %d error\r\n", ret);
    }

    return ret;
}


s32 reportToPultComplex(const u8 *data,
                        const s16 datalen,
                        const PultMessageCode msg_code,
                        const PultMessageQualifier msg_qualifier)
{

    // FIXME: SysF_DeviceConfiguring
//    if (Ar_System_checkFlag(SysF_DeviceConfiguring))
//    {
//        if( CONFIGURATION_PACKET_MARKER != data[SPS_MARKER])
//        {
//           OUT_DEBUG_2("reportToPultComplex() -  NO CONFIGURATION_PACKET_MARKER\r\n");
//           return ERR_INVALID_PARAMETER;
//        }
//    }


    OUT_DEBUG_2("reportToPultComplex(q=%d, xyz=%d, len=%d)\r\n",
                msg_qualifier, msg_code, datalen);

    if (!data || 0 == datalen) {
        OUT_DEBUG_1("Empty message. Aborted.\r\n");
        return ERR_INVALID_PARAMETER;
    }

    s32 ret = 0;
    const u8 maxbodylen = getMessageBuilder(0)->codec->maxMsgBodyLen();
    const u8 nMessages = datalen / maxbodylen + (datalen % maxbodylen ? 1 : 0);
    const u8 *datablock = data;

    for (u8 i = 0; i < nMessages; ++i)
    {
        PultMessage m = {0};

        m.bComplex = TRUE;
        m.msg_code = msg_code;
        m.msg_qualifier = msg_qualifier;
        m.group_id = 0;
        m.identifier = 0;
        m.msg_type   = PMT_Normal;
        m.account = 0;

        u8 part_len = (i + 1 < nMessages) ? maxbodylen : (datalen - ((nMessages - 1) * maxbodylen));

        m.complex_msg_part = Ql_MEM_Alloc(part_len);
        if (!m.complex_msg_part) {
            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", part_len);
            return ERR_GET_NEW_MEMORY_FAILED;
        }

        Ql_memcpy(m.complex_msg_part, datablock, part_len);

        m.part_len = part_len;
        m.frameNo = i;
        m.frameTotal = nMessages - 1; // begins from 0

        ret = pultTxBuffer.enqueue(&m);
        if (ret < RETURN_NO_ERRORS) {
            Ql_MEM_Free(m.complex_msg_part);
            OUT_DEBUG_1("pultTxBuffer::enqueue() = %d error\r\n", ret);
            return ret;
        }

        datablock += part_len;
    }

    return ret;
}
