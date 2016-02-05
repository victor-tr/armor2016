#include "pult_rx_buffer.h"
#include "pult/pult_requests.h"

#include "codecs/codec_armor.h"

#include "core/timers.h"

#include "service/helper.h"


static void pultRxBuffer_enqueue_callback(void);
static void pultRxBuffer_msg_cleanup_callback(PultMessageRx *msg);

REGISTER_FIFO_C(PultMessageRx, pultRxBuffer, pultRxBuffer_enqueue_callback, pultRxBuffer_msg_cleanup_callback)


void pultRxBuffer_enqueue_callback(void)
{
//    if (pTail->d.frameNo != pTail->d.frameTotal)
//        return;

}

void pultRxBuffer_msg_cleanup_callback(PultMessageRx *msg)
{ if (msg && msg->bComplex && msg->complex_msg_part) Ql_MEM_Free(msg->complex_msg_part); }


bool Ar_PultBufferRx_isEntireMessageAvailable(void)
{
    return pultRxBuffer.size() > 1 ||
            (pultRxBuffer.size() == 1 && pTail->d.frameNo == pTail->d.frameTotal);
}


s32 Ar_PultBufferRx_appendMsgFrame(PultMessageRx *frame)
{
    OUT_DEBUG_2("Ar_PultBufferRx_appendMsgFrame()\r\n");

    /* if previous message was failed => remove it before building new message */
    if (pTail && pTail->d.frameNo != pTail->d.frameTotal && frame->frameNo <= pTail->d.frameNo)
        pultRxBuffer.removeAt(pultRxBuffer.size() - 1);

    const bool bFirstFrame = 0 == frame->frameNo;

    /* append a frame or a new message */
    if (bFirstFrame) {
        /* NOTE: can gprs and dtmf and other types have different size of pkt ??? */
        const u8 maxbodylen = getMessageBuilder(0)->codec->maxMsgBodyLen();
        const u16 entire_msg_len = (frame->frameTotal + 1) * maxbodylen;
        const u8 *temp_data = frame->complex_msg_part;

        frame->complex_msg_part = Ql_MEM_Alloc(entire_msg_len);
        if (!frame->complex_msg_part) {
            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", entire_msg_len);
            return ERR_GET_NEW_MEMORY_FAILED;
        }

        Ql_memcpy(frame->complex_msg_part, temp_data, frame->part_len);

        s32 ret = pultRxBuffer.enqueue(frame);
        if (ret < RETURN_NO_ERRORS) {
            pultRxBuffer_msg_cleanup_callback(frame);
            OUT_DEBUG_1("pultRxBuffer::enqueue() = %d error\r\n", ret);
            return ret;
        }
    }
    else if (frame->bComplex && pTail)  // append to the rx buffer tail
    {
        pTail->d.frameNo = frame->frameNo; // update last received frame number
        Ql_memcpy(&pTail->d.complex_msg_part[pTail->d.part_len], frame->complex_msg_part, frame->part_len);
        pTail->d.part_len += frame->part_len;
    }

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++
if(pTail){
OUT_DEBUG_7("rx len: %d\r\n", pTail->d.part_len);
Ar_Helper_debugOutDataPacket(pTail->d.complex_msg_part, pTail->d.part_len);
}
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++

    /* start processing of the rx pult buffer */
    if ((pHead && frame->frameNo == frame->frameTotal) ||
            pultRxBuffer.size() > 1)
    {
        if (QueueStatus_ReadyToProcessing == pultRxBuffer.status()) {
            pultRxBuffer.setStatus(QueueStatus_InProcessing);
            fa_StartPultRxBufferProcessing();
        }
    }

    return RETURN_NO_ERRORS;
}


s32 processPultMessageRx(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("processPultMessageRx()\r\n");

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
OUT_DEBUG_7("remain messages: %d\r\n", pultRxBuffer.size());
OUT_DEBUG_7("q: %d, xyz: %d, group: %d, user: %d\r\n",
            pMsg->msg_qualifier, pMsg->msg_code, pMsg->group_id, pMsg->identifier);
Ar_Helper_debugOutDataPacket(pMsg->complex_msg_part, pMsg->part_len);
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    if (PMQ_AuxInfoMsg != pMsg->msg_qualifier)
        return RETURN_NO_ERRORS;

    switch (pMsg->msg_code) {
    case PMC_CommandReject:
        return rx_handler_requestReject(pMsg);
    case PMC_ConnectionTest:
        return rx_handler_connectionTest(pMsg);
    case PMC_Request_DeviceInfo:
        return rx_handler_deviceInfo(pMsg);
    case PMC_Request_DeviceState:
        return rx_handler_deviceState(pMsg);
    case PMC_Request_LoopsState:
        return rx_handler_loopsState(pMsg);
    case PMC_Request_GroupsState:
        return rx_handler_groupsState(pMsg);
    case PMC_Request_ReadTable:
        return rx_handler_readTable(pMsg);
    case PMC_Request_SaveTableSegment:
        return rx_handler_saveTableSegment(pMsg);
    case PMC_Request_VoiceCall:
        return rx_handler_requestVoiceCall(pMsg);
    case PMC_Request_Apply:
        break;
    case PMC_Request_Cancel:
        break;
    case PMC_Request_PultSettingsData:
        return rx_handler_settingsData(pMsg);
    case PMC_Request_ArmedGroup:
        return rx_handler_armedGroup(pMsg);
    case PMC_Request_DisarmedGroup:
        return rx_handler_disarmedGroup(pMsg);
    case PMC_Request_BatteryVoltage:
        return rx_handler_batteryVoltage(pMsg);
    default:
        break;
}

    return RETURN_NO_ERRORS;
}
