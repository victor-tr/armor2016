#ifndef CONNECTION_H
#define CONNECTION_H

#include "ql_type.h"
#include "common/defines.h"


typedef enum {
    PCHT_GPRS_UDP,
    PCHT_GPRS_TCP,
    PCHT_VOICECALL,
    PCHT_CSD,
    PCHT_SMS,

    PCHT_MAX
} PultChannelType;

typedef enum {
    PSS_CLOSED,
    PSS_ESTABLISHING,
    PSS_WAIT_FOR_CHANNEL_ESTABLISHED,
    PSS_ESTABLISHED,
    PSS_FAILED,
    PSS_DELAYED_AFTER_FAIL,
    PSS_CHANNEL_TYPE_SWITCHING,
    PSS_WAIT_FOR_SIMCARD_SWITCHED
} PultSessionState;

typedef enum {
    SIM_1 = 0,
    SIM_2 = 1
} SIMSlot;

typedef enum  {
    Allowed_SIM_1 = 1,
    Allowed_SIM_2 = 2,
    Allowed_SIM_1_AND_SIM_2 = 3
} Allowed_SIMCard;

typedef struct  {
    PultChannelType  type;
    PultSessionState state;
    u8               attempt;
    u8               index;
    bool             busy;
} PultChannel;


// *********************************************************************
typedef struct _IConnection {
    s32 (*establish)(void);
    s32 (*close)(void);
    s32 (*trackEstablishing)(void);

    bool (*canCurrentChannelTypeBeUsed)(void);
    bool (*canCurrentChannelItemBeUsed)(void);
    bool (*isLastChannelItemForType)(void);
    bool (*isLastAttemptForChannelItem)(void);
} IConnection;

// --
void notifyConnectionInterrupted(PultChannelType type);
void notifyConnectionEstablishingFailed(PultChannelType type);
void notifyConnectionValidIncoming(PultChannelType type);


// *********************************************************************
void notifyProtocolFail(PultChannelType type);
void notifyProtocolEndOk(PultChannelType type);

//
s32 start_pult_protocol_timer(u32 milliseconds);
s16 stop_pult_protocol_timer(void);


// *********************************************************************
void stashPultChannel(void);
void stashPopPultChannel(void);

void setPultChannelBusy(bool busy);
void updatePultChannel(PultChannelType type, u8 index);


//
s32 preinitPultConnections(void);
const PultChannel *getChannel(void);

void resetChannelAttemptsCounter(void);

//
void fa_SendMessageToPult(void);
void pultSession_timerHandler(void *data);

//
void fa_StartPultRxBufferProcessing(void);
void pultRxBuffer_timerHandler(void *data);

void ArmingACK_ArmedLEDSwitchON(void);



#endif // CONNECTION_H
