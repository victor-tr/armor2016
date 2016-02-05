#include "voicecall.h"

#include "ql_stdlib.h"
#include "ql_system.h"

#include "ql_power.h"

#include "common/debug_macro.h"

#include "core/simcard.h"
#include "core/atcommand.h"
#include "core/system.h"

#include "pult/connection.h"

#include "db/db.h"

#include "pult/pult_tx_buffer.h"
#include "pult/pult_rx_buffer.h"

#define DTMF_BUFFER_SIZE    300


static bool _incoming_call_processed = FALSE;

static u16 dtmfCounter = 0;


static void _callHangup(void)
{
    OUT_DEBUG_1("_callHangup\r\n");
    s32 ret = RIL_Telephony_Hangup();
    if (ret != RIL_AT_SUCCESS)
        OUT_DEBUG_1("RIL_Telephony_Hangup() = %d Enum_ATSndError\r\n", ret);
    _incoming_call_processed = FALSE;
}


static s32 AT_Response_CLCC_Handler(char* line, u32 len, void* userdata)
{
    //OUT_DEBUG_1("CLCC_Handler: ");
    char *head = NULL;

    if (Ql_StrPrefixMatch(line, "\r\n+CLCC:"))
    {
        //OUT_UNPREFIXED_LINE("+CLCC ");

        ST_Call_Parameters *call = userdata;

        head = Ql_strchr(line, ',') + 1;
        call->direction = Ql_atoi(head);
        //OUT_UNPREFIXED_LINE("call->direction:%d ", call->direction);

        head = Ql_strchr(head, ',') + 1;
        call->status = Ql_atoi(head);
        //OUT_UNPREFIXED_LINE("call->status:%d ", call->status);

        head = Ql_strchr(head, ',') + 1;
        call->mode = Ql_atoi(head);
        //OUT_UNPREFIXED_LINE("call->mode:%d ", call->mode);

        head = Ql_strchr(head, '"') + 1;
        char *tail = Ql_strchr(head, '"');
        Ql_strncpy(call->phoneNumber, head, tail - head);
        //OUT_UNPREFIXED_LINE("call->phoneNumber:%s ", call->phoneNumber);

        call->type = Ql_atoi(tail + 2);
        call->isValid = TRUE;
        //OUT_UNPREFIXED_LINE("call->isValid:%d \r\n", call->isValid);

        return RIL_ATRSP_CONTINUE;
    }

    head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {
        //OUT_UNPREFIXED_LINE("OK\r\n");

        return  RIL_ATRSP_SUCCESS;
    }

    if(Ql_RIL_FindLine(line, len, "ERROR") ||
           Ql_RIL_FindString(line, len, "+CME ERROR:") ||
           Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        //OUT_UNPREFIXED_LINE("ERROR\r\n");

        return RIL_ATRSP_FAILED;
    }
    //OUT_UNPREFIXED_LINE("RIL_ATRSP_CONTINUE\r\n");

    return RIL_ATRSP_CONTINUE;
}

static s32 getCallStatus(ST_Call_Parameters *call)
{
    char *cmd = "AT+CLCC";
    s32 ret = Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), AT_Response_CLCC_Handler, call, 0);
    if (ret != RIL_AT_SUCCESS) {
        OUT_DEBUG_1("Ql_RIL_SendATCmd(%s) = %d error\r\n", cmd, ret);
        return ERR_AT_COMMAND_FAILED;
    }

    OUT_DEBUG_ATCMD("getCallStatus(): isValid=%s, status=%d, type=%d, number=%s, dir=%s, mode=%d\r\n",
                    call->isValid ? "Yes" : "No", call->status, call->type, call->phoneNumber,
                    call->direction ? "IN" : "OUT", call->mode);
    return RETURN_NO_ERRORS;
}



s32 dialPhoneNumber(char* callNo)
{
    s32 dialing_result = 0;
    s32 ret = RIL_Telephony_Dial(0, callNo, &dialing_result);
    OUT_DEBUG_2("dialPhoneNumber(number=\"%s\"): result = %d\r\n", callNo, dialing_result);

    if (RIL_AT_SUCCESS == ret)
        setPultChannelBusy(TRUE);
    else
        OUT_DEBUG_1("RIL_Telephony_Dial() = %d Enum_ATSndError\r\n", ret);

    return ret;
}

void endVoiceCall(void)
{
    OUT_DEBUG_2("endVoiceCall()\r\n");
    _callHangup();
}



static struct DtmfRxBuffer
{
    u8 data[DTMF_BUFFER_SIZE];
    u16 size;
}
_rx_buffer;

void append_dtmf_digit(u8 dtmf_digit)
{ _rx_buffer.data[_rx_buffer.size++] = dtmf_digit; }

void clear_dtmf_rx_buffer(void)
{ Ql_memset(_rx_buffer.data, 0, DTMF_BUFFER_SIZE); _rx_buffer.size = 0; }

u8 *get_dtmf_rx_packet(void)
{ return _rx_buffer.data; }



/* for ukrainian (code 380) 13-digit phone numbers only */
static char *normalizePhoneNumber(const char *phone)
{
    char *num = Ql_strstr(phone, "380");
    if (num && Ql_strlen(num += 2) == 10) return num;
    else return NULL;
}


s32 Callback_Dial(void)
{
    OUT_DEBUG_2("Callback_Dial()\r\n");

    static bool runOnce = FALSE;

    ST_Call_Parameters call = {0};
    s32 ret = getCallStatus(&call);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getCallStatus() = %d error\r\n", ret);
    }

    if (!call.isValid) {
        OUT_DEBUG_1("Dialing error: No active voice call found\r\n");
    }
    else {
        switch (call.status) {
        case CallStat_Active:
            stop_pult_protocol_timer();
            return ERR_ALREADY_ESTABLISHED;

        case CallStat_MO_Dialing:
            runOnce = TRUE;
            return ERR_SHOULD_WAIT;

        case CallStat_MO_Alerting:
            if (runOnce) {
                runOnce = FALSE;
                start_pult_protocol_timer(VOICECALL_NO_ANSWER_HANGUP_INTERVAL);
            }
            return ERR_SHOULD_WAIT;

        default:
            break;
        }
    }

    _callHangup();
    return ERR_NO_ACTIVE_CALL_FOUND;
}

void Callback_Ring(char* coming_call_num)
{
    /* process an incoming call only one time */
    if (_incoming_call_processed) return;
    else _incoming_call_processed = TRUE;

    OUT_DEBUG_2("Callback_Ring(): %s\r\n", coming_call_num);

    if (getChannel()->busy ||
            !Ar_System_checkFlag(SysF_DeviceInitialized) ||
            Ar_System_checkFlag(SysF_DeviceConfiguring))
    {
        _callHangup();
// ++++++++++++++++++++++++++++++++++++++++++++
OUT_DEBUG_8("busy: %d, initialized: %d, configuring: %d\r\n",
            getChannel()->busy,
            Ar_System_checkFlag(SysF_DeviceInitialized),
            Ar_System_checkFlag(SysF_DeviceConfiguring));
// ++++++++++++++++++++++++++++++++++++++++++++
        return;
    }

    SIMSlot sim;
    s16 idx = -1;

    char *number = normalizePhoneNumber(coming_call_num);
    if (number) idx = findPhoneIndex(number, &sim);

    if (-1 == idx) {
        _callHangup();
        OUT_DEBUG_3("Normalized number \"%s\" was not found in DB. Call rejected.\r\n", number);
        return;
    }



    /*
     * other (not the current) SIM may not have any valid data at 'idx' position,
     * so we need remember all significant parameters to restore channel state
     * in this case
     */
    if (Ar_SIM_currentSlot() != sim)
        stashPultChannel();

    updatePultChannel(PCHT_VOICECALL, idx);

    s32 answer_result = 0;
    s32 ret = RIL_Telephony_Answer(&answer_result);
    OUT_DEBUG_3("RIL_Telephony_Answer(): result = %d\r\n", answer_result);
    if (ret != RIL_AT_SUCCESS) {
        OUT_DEBUG_1("RIL_Telephony_Answer() = %d Enum_ATSndError\r\n", ret);
        return;
    }

    setPultChannelBusy(TRUE);
    notifyConnectionValidIncoming(PCHT_VOICECALL);
}

void Callback_Hangup(void)
{
    OUT_DEBUG_2("Callback_Hangup()\r\n");

    _incoming_call_processed = FALSE;
    notifyConnectionInterrupted(PCHT_VOICECALL);
}


// ******************************************************
//          Send DTMF
// ******************************************************
static s32 AT_Response_send_QWDTMF_Handler(char* line, u32 len, void* userData)
{
    static s32 playcode = 0;
    char cmd[100] = {0};

    char *head = Ql_RIL_FindString(line, len, "+QWDTMF: ");   // continue wait
    if (head) {
        char *str = Ql_strchr(head, ' ');
        if (str){

            playcode = Ql_atoi(str+1);
        }
        return RIL_ATRSP_CONTINUE;
    }

    head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {

        *(bool *)userData = (5 == playcode);    // about 5 => see the GSM-module AT commands manual
        playcode = 0;
        return  RIL_ATRSP_SUCCESS;
    }

    if(Ql_RIL_FindLine(line, len, "ERROR") ||
           Ql_RIL_FindString(line, len, "+CME ERROR:") ||
           Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {

        *(bool *)userData = FALSE;
        playcode = 0;
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE;
}

s32 generateDtmf(u8 *dtmf, u16 len)
{
    OUT_DEBUG_2("generateDtmf(len=%d)\r\n", len);
    OUT_DEBUG_2("--------------------------------------------------------\r\n");
    OUT_DEBUG_2("pultTxBuffer.size = %d\r\n", pultTxBuffer.size());
    OUT_DEBUG_2("DTMF send = %d\r\n", ++dtmfCounter);
    //   "AT+QWDTMF=7,0,\"1401370077000017,220,140\"";
    //    Ql_sprintf(cmd, "AT+VTS=\"1,4,0,1,3,7,0,0,7,7,0,0,0,0,1,7\"");

    OUT_DEBUG_2("--------------------------------------------------------\r\n");

    /* terminate valid dtmf data because it will be treated as C-string */
    dtmf[len] = 0;
    char cmd[DTMF_BUFFER_SIZE] = {0};

    s32 ret = 0;
    bool result = FALSE;

    CommonSettings *pCS = getCommonSettings(0);
    Ql_sprintf(cmd, "AT+QWDTMF=%d,%d,\"%s,%d,%d\"", 7, 0, dtmf, pCS->dtmf_pulse_ms, pCS->dtmf_pause_ms);

    OUT_DEBUG_ATCMD("%s\r\n", cmd);

    ret = Ql_RIL_SendATCmd(cmd, Ql_strlen(cmd), AT_Response_send_QWDTMF_Handler, &result, 0);

    if (!result)
        OUT_DEBUG_1("DTMF sending result: FAIL\r\n");

    if (RIL_AT_SUCCESS != ret) {
        OUT_DEBUG_1("Ql_RIL_SendATCmd(%s) = %d Enum_ATSndError\r\n", cmd, ret);
        return ERR_AT_COMMAND_FAILED;
    }

    return RETURN_NO_ERRORS;
}
