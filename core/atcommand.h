#ifndef ATCOMMAND_H
#define ATCOMMAND_H

#include "ql_type.h"
#include "ril.h"
#include "ril_util.h"


typedef struct {
    void (*CallBack_SimCardUnregistered)(bool success);
    void (*CallBack_SimCardRegistered)(bool success);
    void (*CallBack_SimCardPresenceChecked)(bool inserted);
    void (*CallBack_SimCardSlotChanged)(bool success);
    void (*CallBack_SimCardGetCurrentSlot)(u8 currentSlot);
} SimCard_AT_Callback_t;

void register_AT_callbacks(SimCard_AT_Callback_t *callbacks);


// registered AT-commands codes
typedef enum {
    ATCMD_NoCommand,
    ATCMD_UnregisterSimCard,
    ATCMD_RegisterSimCard,
    ATCMD_EnableDTMFDecoder,
    ATCMD_CheckSimCardPresence,
    ATCMD_ActivateSim1,
    ATCMD_ActivateSim2,
    ATCMD_GetCurrentSimSlot,
    ATCMD_SetMicrophon,
    ATCMD_SetLoudSpeaker,
    ATCMD_SetVolumeMax,
    ATCMD_GetTime,
    ATCMD_ObtainLastTime,
    ATCMD_SaveTime,
    ATCMD_VDT,

    ATCMD_MAX
} ATCommandIdx;


s32 AT_Response_Main_Handler(char* line, u32 len, void* data);
s32 send_AT_cmd_by_code(ATCommandIdx cmdcode);


#endif // ATCOMMAND_H
