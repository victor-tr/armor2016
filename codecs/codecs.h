#ifndef CODECS_H
#define CODECS_H

#include "ql_type.h"
#include "ril_telephony.h"

#include "message_codes.h"

#include "pult/connection.h"

#include "common/configurator_protocol.h"

typedef struct _IProtocol {
    s32 (*startSendProtocol)(void);
    s32 (*startRecvProtocol)(void);
    s32 (*stopProtocol)(void);
    bool (*isActive)(void);
    void (*timerHandler)(void *data);
} IProtocol;

void registerProtocol(PultChannelType type, IProtocol *protocol);
IProtocol *getProtocol(PultChannelType type);


// *********************************************************************
typedef struct _PultMessageCodec {
    IProtocol *protocols[PCHT_MAX];
    PultMessageCodecType  type;

    u16  enc_map_1[ENC_CODEC_TBL_SIZE];     // new message or set disarmed
    u16  enc_map_3[ENC_CODEC_TBL_SIZE];     // new restoral or set armed

    u8  txBuffer[CODEC_TX_BUFFER_SIZE];
    s32 (*sends[PCHT_MAX])(void);

    s32 (*readDtmf)(u8 dtmfCode);
    void (*notifyDtmfDigitReceived)(u8 digit);

    s32 (*readUdp)(u8 *data, s16 len);
    void (*notifyUdpDatagramReceived)(u8 *data, u16 len);

    u16 (*maxMsgBodyLen)(void);
    u16 (*SPSMarkerPosition)(void);
    u16 (*SPSAuxInfoPosition)(void);
    u16 (*SPSACKPosition)(void);
} PultMessageCodec;


typedef struct _PultMessageBuilder PultMessageBuilder;
void init_pultMessageBuilder(PultMessageBuilder *builder, PultMessageCodecType codecType);


#endif // CODECS_H
