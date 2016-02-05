#include "urc.h"

#include "pult/connection.h"
#include "pult/voicecall.h"

#include "common/debug_macro.h"

#include "db/db.h"

#include "core/system.h"

#include "service/humanize.h"

#include "ril_util.h"


void OnURCHandler_RecvDTMF(const char* strURC, /*[out]*/void* reserved)
{
    if (Ql_StrPrefixMatch(strURC, "\r\n+QTONEDET: "))
    {
        char *value = Ql_strstr(strURC, " ");
        u32 deccode=Ql_atoi(value+1);
        s32 ret = Ql_OS_SendMessage(main_task_id, MSG_ID_URC_INDICATION,URC_DTMF_RECEIVED,deccode);
        if (ret != OS_SUCCESS) {
            OUT_DEBUG_1("Ql_OS_SendMessage(URC_DTMF_RECEIVED) = %d error\r\n", ret);
        }
    }
}


void OnURCHandler_Call_Answered(const char* strURC, /*[out]*/void* reserved)
{
    if (Ql_StrPrefixMatch(strURC, "\r\n+COLP: "))
    {
        ST_ComingCall* pOutCall =Ql_MEM_Alloc(sizeof(ST_ComingCall));
        if (!pOutCall) {
            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", sizeof(ST_ComingCall));
            return;
        }
        Ql_memset((void*)pOutCall, 0x0, sizeof(ST_ComingCall));

        char *p2 = Ql_strchr(strURC, ',');
        pOutCall->type = Ql_atoi(p2 + 1);

        char *p1 = Ql_strchr(strURC, ':');
        Ql_memcpy(pOutCall->phoneNumber, p1 + 1, p2 - (p1 + 1));

        s32 ret = Ql_OS_SendMessage(main_task_id, MSG_ID_URC_INDICATION,URC_OUTGOING_CALL_ANSWERED, (u32)&pOutCall);
        if (ret != OS_SUCCESS) {
            OUT_DEBUG_1("Ql_OS_SendMessage(URC_OUTGOING_CALL_ANSWERED) = %d error\r\n", ret);
        }
    }
}



/***************************************************************
*   global URC-results handler
****************************************************************/
void RIL_URC_Handler(u32 type, u32 data)
{
    //OUT_UNPREFIXED_LINE("\r\n");
    //OUT_UNPREFIXED_LINE("<-- Received URC: type: %d, -->\r\n", type);

    switch (type)
    {
    case URC_SYS_INIT_STATE_IND:
    {
        OUT_UNPREFIXED_LINE("<-- Sys Init Status %d -->\r\n", data);
        if (SYS_STATE_PHBOK == data)
        {
            ;
        }
        break;
    }
    case URC_SIM_CARD_STATE_IND:
    {

        OUT_UNPREFIXED_LINE("<-- SIM Card Status: %d -->\r\n", data);
        break;
    }
    case URC_GSM_NW_STATE_IND:
        OUT_UNPREFIXED_LINE("<-- GSM Network Status: %d -->\r\n", data);
        break;
    case URC_GPRS_NW_STATE_IND:
        OUT_UNPREFIXED_LINE("<-- GPRS Network Status: %d -->\r\n", data);
        break;
    case URC_CFUN_STATE_IND:
        OUT_UNPREFIXED_LINE("<-- CFUN Status: %d -->\r\n", data);
        break;
    case URC_COMING_CALL_IND:
    {
        //MSG_ID_VOICECALL_INCOMING_RING
        ST_ComingCall* pComingCall =(ST_ComingCall*)data;
        OUT_UNPREFIXED_LINE("<-- Coming call, number:%s, type:%d -->\r\n",
                       pComingCall->phoneNumber, pComingCall->type);
        Callback_Ring(pComingCall->phoneNumber);
        break;
    }
    case URC_OUTGOING_CALL_ANSWERED:
    {
        ST_ComingCall *pOutCall = (ST_ComingCall*)data;
        OUT_UNPREFIXED_LINE("<-- Outgoing call answered, number:%s, type:%d -->\r\n",
                       pOutCall->phoneNumber, pOutCall->type);
        Ql_MEM_Free(pOutCall);
        break;
    }
    case URC_CALL_STATE_IND:
    {
        //MSG_ID_VOICECALL_INTERRUPTED
        u32 callState = data;
        OUT_UNPREFIXED_LINE("<-- Call state: %d -->\r\n", callState);
        Callback_Hangup();
        break;
    }
    case URC_NEW_SMS_IND:
        OUT_UNPREFIXED_LINE("<-- New SMS Arrives: index=%d -->\r\n", data);
        break;
    case URC_MODULE_VOLTAGE_IND:
        OUT_UNPREFIXED_LINE("<-- VBatt Voltage Ind: type=%d -->\r\n", data);
        break;
    case URC_DTMF_RECEIVED:
    {
        //MSG_ID_DTMF_CODE_RECEIVED
        OUT_UNPREFIXED_LINE("<-- DTMF received -->\r\n");
        u32 deccode = data;
        OUT_DEBUG_8("DTMF>>> received decimal: %d\r\n", deccode);
        if (deccode >= 35 && deccode <= 68)   // ascii from '#' to 'D'
        {
            s32 ret = getMessageBuilder(NULL)->readDtmfCode(deccode);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("Codec %s: readDtmfCode() = %d error\r\n", getCodec_humanize(), ret);
            }
        }
        break;
    }
    case URC_END:
    case URC_SYS_BEGIN:
    case URC_SYS_END:
        break;

    default:
        OUT_UNPREFIXED_LINE("<-- Other URC: type=%d -->\r\n", type);
        break;
    }
}
