#include "atcommand.h"

#include "ql_stdlib.h"
#include "ql_uart.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "core/simcard.h"
#include "core/system.h"
#include "core/timers.h"

#include "codecs/codecs.h"

#include "pult/voicecall.h"


static SimCard_AT_Callback_t *_callbacks = NULL;

void register_AT_callbacks(SimCard_AT_Callback_t *callbacks)
{
    OUT_DEBUG_2("register_AT_callbacks()\r\n");
    _callbacks = callbacks;
}


// -------------------------------------------------------------
//                              RESPONSE HANDLERS
// -------------------------------------------------------------

// ATCMD_UnregisterSimCard
static s32 AT_Response_CFUN1_Handler(char* line, u32 len, void* userData)
{
    static bool bInserted = TRUE;

    char *head = Ql_RIL_FindLine(line, len, "+CPIN: NOT INSERTED");
    if (head) {
        bInserted = FALSE;
    }

    head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {
        _callbacks->CallBack_SimCardRegistered(bInserted);
        bInserted = TRUE;
        return  RIL_ATRSP_SUCCESS;
    }

    head = Ql_RIL_FindLine(line, len, "ERROR");
    if(head) {
        _callbacks->CallBack_SimCardRegistered(FALSE);
        bInserted = TRUE;
        return  RIL_ATRSP_FAILED;
    }

    if(Ql_RIL_FindString(line, len, "+CME ERROR:") ||
        Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        _callbacks->CallBack_SimCardRegistered(FALSE);
        bInserted = TRUE;
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE;
}

// ATCMD_UnregisterSimCard
static s32 AT_Response_CFUN0_Handler(char* line, u32 len, void* userData)
{
    OUT_DEBUG_1("AT_Response_CFUN0_Handler\r\n");

    char *head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {
        OUT_DEBUG_1("OK\r\n");
        _callbacks->CallBack_SimCardUnregistered(TRUE);
        return  RIL_ATRSP_SUCCESS;
    }

    head = Ql_RIL_FindLine(line, len, "ERROR");
    if(head) {
        OUT_DEBUG_1("ERROR\r\n");
        _callbacks->CallBack_SimCardUnregistered(FALSE);
        return  RIL_ATRSP_FAILED;
    }

    if(Ql_RIL_FindString(line, len, "+CME ERROR:") ||
        Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        OUT_DEBUG_1("+CME ERROR +CMS ERROR\r\n");
        _callbacks->CallBack_SimCardUnregistered(FALSE);
        return RIL_ATRSP_FAILED;
    }
    OUT_DEBUG_1("RIL_ATRSP_CONTINUE\r\n");

    return RIL_ATRSP_CONTINUE;
}

// ATCMD_CheckSimCardPresence
static s32 AT_Response_QSIMSTAT_Handler(char* line, u32 len, void* userData)
{
    char *head = Ql_RIL_FindString(line, len, "+QSIMSTAT:");   // continue wait
    if (head) {
        char *str = Ql_strchr(head, ',');
        if (str) {
            bool sim_inserted = ('1' == str[1]) ? TRUE : FALSE;
            _callbacks->CallBack_SimCardPresenceChecked(sim_inserted);
        }
        return RIL_ATRSP_CONTINUE;
    }

    head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {
        return  RIL_ATRSP_SUCCESS;
    }

    head = Ql_RIL_FindLine(line, len, "ERROR");
    if(head) {
        return  RIL_ATRSP_FAILED;
    }

    if(Ql_RIL_FindString(line, len, "+CME ERROR:") ||
        Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE;
}

// ATCMD_GetCurrentSimSlot
static s32 AT_Response_get_QDSIM_Handler(char* line, u32 len, void* userData)
{
    char *head = Ql_RIL_FindString(line, len, "+QDSIM: ");   // continue wait
    if (head) {
        char *str = Ql_strchr(head, ' ');
        if (str) {
            const SIMSlot current_slot = Ql_atoi(str+1) == 0 ? SIM_1 : SIM_2;
            _callbacks->CallBack_SimCardGetCurrentSlot(current_slot);
        }
        return RIL_ATRSP_CONTINUE;
    }

    head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {
        return  RIL_ATRSP_SUCCESS;
    }

    head = Ql_RIL_FindLine(line, len, "ERROR");
    if(head) {
        return  RIL_ATRSP_FAILED;
    }

    if(Ql_RIL_FindString(line, len, "+CME ERROR:") ||
        Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE;
}

// ATCMD_ActivateSim1
// ATCMD_ActivateSim2
static s32 AT_Response_set_QDSIM_Handler(char* line, u32 len, void* userData)
{
    char *head = Ql_RIL_FindLine(line, len, "OK");
    if(head) {
        _callbacks->CallBack_SimCardSlotChanged(TRUE);
        return  RIL_ATRSP_SUCCESS;
    }

    if(Ql_RIL_FindLine(line, len, "ERROR") ||
           Ql_RIL_FindString(line, len, "+CME ERROR:") ||
           Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        _callbacks->CallBack_SimCardSlotChanged(FALSE);
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE;
}


// ***********************************************************

s32 AT_Response_Main_Handler(char* line, u32 len, void* data)
{
    OUT_DEBUG_ATCMD("%s", line);

    if (Ql_RIL_FindLine(line, len, "OK"))
    {
        return  RIL_ATRSP_SUCCESS;
    }
    else if (Ql_RIL_FindLine(line, len, "ERROR"))
    {
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CME ERROR"))
    {
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE; //continue wait
}


// ***********************************************************

typedef struct {
    char cmd[100];
    s32 (*at_response_handler)(char* line, u32 len, void* data);
    void *result;
} AtCmdContainer;

static AtCmdContainer _atcmds[] =
{
    {"", NULL, NULL},     // ATCMD_NoCommand
    {"AT+CFUN=0", AT_Response_CFUN0_Handler, NULL},  // ATCMD_UnregisterSimCard
    {"AT+CFUN=1", AT_Response_CFUN1_Handler, NULL},  // ATCMD_RegisterSimCard
    {"AT+QTONEDET=1;+QTONEDET=4,1,3,3,65536", NULL, NULL},  // ATCMD_EnableDTMFDecoder
    {"AT+QSIMSTAT?", AT_Response_QSIMSTAT_Handler, NULL},    // ATCMD_CheckSimCardPresence
    {"AT+QDSIM=0", AT_Response_set_QDSIM_Handler, NULL},       // ATCMD_ActivateSim1
    {"AT+QDSIM=1", AT_Response_set_QDSIM_Handler, NULL},        // ATCMD_ActivateSim2
    {"AT+QDSIM?", AT_Response_get_QDSIM_Handler, NULL},  // ATCMD_GetCurrentSimSlot
    {"AT+QMIC=2", AT_Response_Main_Handler, NULL},  // ATCMD_SetMicrophon
    {"AT+QAUDCH=2", AT_Response_Main_Handler, NULL},  // ATCMD_SetLoudSpeaker
    {"AT+CLVL=100", AT_Response_Main_Handler, NULL},  // ATCMD_SetVolumeMax
    {"AT+QNITZ=1", AT_Response_Main_Handler, NULL},  // ATCMD_GetTime
    {"AT+QLTS", AT_Response_Main_Handler, NULL},  // ATCMD_ObtainLastTime
    {"AT+CTZU=1", AT_Response_Main_Handler, NULL},  // ATCMD_SaveTime
    {"AT+VTD=1", AT_Response_Main_Handler, NULL},  // ATCMD_VDT duration
};



s32 send_AT_cmd_by_code(ATCommandIdx cmdcode)
{
    if (cmdcode >= ATCMD_MAX || ATCMD_NoCommand == cmdcode) {
        OUT_DEBUG_1("send_AT_cmd_by_code(%d): bad AT-command code\r\n", cmdcode);
        return ERR_BAD_AT_COMMAND;
    }

    OUT_DEBUG_2("send_AT_cmd_by_code(code=%d): \"%s\"\r\n", cmdcode, _atcmds[cmdcode].cmd);

    s32 ret = Ql_RIL_SendATCmd(_atcmds[cmdcode].cmd,
                               Ql_strlen(_atcmds[cmdcode].cmd),
                               _atcmds[cmdcode].at_response_handler,
                               _atcmds[cmdcode].result,
                               0);

    switch (ret) {
    case RIL_AT_FAILED:
        OUT_DEBUG_1("send_AT_cmd_by_code(cmdcode=%d) FAILED\r\n", cmdcode);
        break;
    case RIL_AT_BUSY:
        OUT_DEBUG_1("send_AT_cmd_by_code(cmdcode=%d): BUSY\r\n", cmdcode);
        break;
    case RIL_AT_TIMEOUT:
        OUT_DEBUG_1("send_AT_cmd_by_code(cmdcode=%d): TIMEOUT\r\n", cmdcode);
        break;
    case RIL_AT_INVALID_PARAM:
        OUT_DEBUG_1("send_AT_cmd_by_code(cmdcode=%d): INVALID PARAMETER\r\n", cmdcode);
        break;
    case RIL_AT_UNINITIALIZED:
        OUT_DEBUG_1("send_AT_cmd_by_code(cmdcode=%d): RIL UNINITIALIZED\r\n", cmdcode);
        break;
    case RIL_AT_SUCCESS:
        break;
    }
    return ret;
}
