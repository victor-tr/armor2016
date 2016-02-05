#ifndef CODEC_ARMOR_H
#define CODEC_ARMOR_H

#include "ql_type.h"
#include "codecs.h"


typedef enum {
    ADMH_FrameLen_H,    // 18..99
    ADMH_FrameLen_L,
    ADMH_ProtocolVer_H, // 01..99
    ADMH_ProtocolVer_L,
    ADMH_Ident_H,         // 00..99
    ADMH_Ident_L,
    ADMH_FrameNum,      // 0..9
    ADMH_FrameCount,    // 0..9

    ADMH_HeaderLen,

    ADMH_Qualifier = ADMH_HeaderLen,
    ADMH_EventCode_x,
    ADMH_EventCode_y,
    ADMH_EventCode_z,
    ADMH_GG_H,
    ADMH_GG_M,
    ADMH_GG_L,
    ADMH_CCC_H,
    ADMH_CCC_M,
    ADMH_CCC_L,

    ADMH_GeneralMsgLen  = 15 // ADMH_HeaderLen + data_in_bytes + CRC
} ArmorDtmfMessageHeader;


typedef enum {
    AGMH_FrameLen,    // 10..99
    AGMH_ProtocolVer, // 01..99
    AGMH_Ident,         // 00..99
    AGMH_FrameNum,      // 0..9
    AGMH_FrameCount,    // 0..9

    AGMH_HeaderLen,

    AGMH_Qualifier_and_X = AGMH_HeaderLen,   // encoded as BCD
    AGMH_Y_and_Z, // encoded as BCD
    AGMH_GG,   // helper fields for GPRS ARMOR alarm messages
    AGMH_CCC,

    AGMH_GeneralMsgLen  = 10,
    AGMH_ACK_Len        = 8
} ArmorGprsMessageHeader;


void init_codec_armor(PultMessageCodec *codec);

u16 realDtmfMsgLen_armor(u8 len_from_header, bool isFirstFrame);
bool check_crc_dtmf_armor(const u8 *pkt);
bool check_crc_gprs_armor(const u8 *pkt);
u16 bcd_2_xyz(u16 bcd);
u8 bcd_2_q(u16 bcd);

s32 checkRxCycleCounterValidity(const u8 value);
void incrementRxCycleCounter(void);
u8 getTxCycleCounter(void);
void incrementTxCycleCounter(void);

void compressDtmf_armor(u8 *pkt, u16 *realLen);
void decompressDtmf_armor(u8 *pkt, u16 *len);


#endif // CODEC_ARMOR_H
