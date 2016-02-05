#ifndef VOICECALL_H
#define VOICECALL_H

#include "ql_type.h"
#include "ril_telephony.h"


typedef enum {
    CallDir_MO_Outgoing = 0,
    CallDir_MT_Incoming = 1
} CallDirection;

typedef enum {
    CallStat_Active      = 0,
    CallStat_Held        = 1,
    CallStat_MO_Dialing  = 2,
    CallStat_MO_Alerting = 3,
    CallStat_MT_Incoming = 4,
    CallStat_MT_Waiting  = 5
} CallStatus;

typedef enum {
    CallMod_Voice   = 0,
    CallMod_Data    = 1,
    CallMod_FAX     = 2,
    CallMod_Unknown = 9
} CallMode;

typedef struct {
    s32  status;
    s32  direction;
    s32  mode;
    s32  type;
    char phoneNumber[15];
    bool isValid;
} ST_Call_Parameters;



/*
 *  return the same codes as Ql_RIL_SendATCmd() func
 */
s32 dialPhoneNumber(char* callNo);
void endVoiceCall(void);


/*
 *  return:
 *          ERR_SHOULD_WAIT
 *          RETURN_NO_ERRORS
 *          ERR_NO_ACTIVE_CALL_FOUND
 */
s32  Callback_Dial(void);
void Callback_Ring(char *coming_call_num);
void Callback_Hangup(void);


void append_dtmf_digit(u8 dtmf_digit);
void clear_dtmf_rx_buffer(void);
u8 *get_dtmf_rx_packet(void);


s32 generateDtmf(u8 *dtmf, u16 len);


#endif // VOICECALL_H
