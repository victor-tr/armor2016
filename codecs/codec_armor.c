#include "codec_armor.h"


#include "common/debug_macro.h"

#include "db/db.h"

#include "pult/pult_tx_buffer.h"
#include "pult/gprs_udp.h"
#include "pult/voicecall.h"
#include "pult/protocol_base_armor.h"
#include "pult/protocol_gprs_armor.h"


#include "service/humanize.h"
#include "service/helper.h"


static PultMessageCodec *_pCodec = NULL;

static u8  _rx_cycle_counter = 0;
static u8  _tx_cycle_counter = 0;   // must be 0 to sync rx and tx counters on the other side


// =============================================
// -- HELPERS
u16 realDtmfMsgLen_armor(u8 len_from_header, bool isFirstFrame)
{
    const u8 nHeaderSymbols = isFirstFrame ? ADMH_HeaderLen + 4 : ADMH_HeaderLen;  // 4 - qxyz in first frame only
    u8 data_field_len = len_from_header - (nHeaderSymbols + 1);
    return data_field_len * 3 + (nHeaderSymbols + 1); // each byte as three digits
}

static u8 evaluate_crc_dtmf_armor(const u8 *buffer)
{
    s16 sum = 0;
    bool bFirstFrame = 0 == buffer[ADMH_FrameNum];
    u16 len = realDtmfMsgLen_armor(buffer[ADMH_FrameLen_H]*10 + buffer[ADMH_FrameLen_L], bFirstFrame) - 1;

    for (u16 i = 0; i < len; ++i)
        sum += buffer[i] ? buffer[i] : 10;

    const u8 basis = 9;
    s16 alt_sum = sum;

    if (sum % basis)
        alt_sum = (sum / basis + 1) * basis;

    u8 crc = alt_sum - sum;

//    if (!ret)         Kudrinski doesn't use it
//        ret = basis;

    return crc;
}

bool check_crc_dtmf_armor(const u8 *pkt)
{
    bool bFirstFrame = 0 == pkt[ADMH_FrameNum];
    u16 len = realDtmfMsgLen_armor(pkt[ADMH_FrameLen_H]*10 + pkt[ADMH_FrameLen_L], bFirstFrame) - 1;
    bool ok = evaluate_crc_dtmf_armor(pkt) == pkt[len];
    OUT_DEBUG_7("DTMF CRC: %s\r\n", ok ? "OK" : "BAD");
    return ok;
}

static u8 evaluate_crc_gprs_armor(const u8 *buffer)
{
    u8 len = buffer[AGMH_FrameLen] - 1;   // without CRC-byte
    u8 crc = 0x00;

    while (len--) {
        crc ^= *buffer++;
        for (u8 i = 0; i < 8; i++)
            crc = crc & 0x01 ? (crc >> 1) ^ 0x8C : crc >> 1;
    }

    return crc;
}

bool check_crc_gprs_armor(const u8 *pkt)
{
    const u8 len = pkt[AGMH_FrameLen];
    bool ok = evaluate_crc_gprs_armor(pkt) == pkt[len-1];
    OUT_DEBUG_7("GPRS CRC: %s\r\n", ok ? "OK" : "BAD");
    return ok;
}


static void resetCycleCounters(void)
{ _rx_cycle_counter = 0; _tx_cycle_counter = 1; }

static bool isCycleCountersInitialized(void)
{ return 0 != _tx_cycle_counter; }


static inline u8 evaluateNextRxCycleCounter(void)
{ return _rx_cycle_counter < PROTOCOL_ARMOR_MAX_CYCLE_COUNTER ? _rx_cycle_counter + 1 : 1; }

//static u8 getRxCycleCounter(void)
//{ return _rx_cycle_counter; }

s32 checkRxCycleCounterValidity(const u8 value)
{
    /* reset counters trigger */
    if (0 == value) {
        resetCycleCounters();
        return RETURN_NO_ERRORS;
    }

    if (isCycleCountersInitialized())
    {
        /* current value => repeated */
        if (value == _rx_cycle_counter)
            return ERR_PREVIOUS_PKT_INDEX;

        /* valid (next) value */
        if (value == evaluateNextRxCycleCounter())
            return RETURN_NO_ERRORS;
    }

    return ERR_BAD_PKT_INDEX;   // invalid cycle counter value
}

void incrementRxCycleCounter(void)
{ _rx_cycle_counter = evaluateNextRxCycleCounter(); }


u8 getTxCycleCounter(void)
{ return _tx_cycle_counter; }

void incrementTxCycleCounter(void)
{ _tx_cycle_counter = _tx_cycle_counter < PROTOCOL_ARMOR_MAX_CYCLE_COUNTER ? _tx_cycle_counter + 1 : 1; }


static u8 digitToDtmf_armor(u8 digit)
{
    u8 ch = 0;

    if (/*digit >= 0 &&*/ digit <= 9)
        ch = (char)(digit + 0x30);
    else {
        switch (digit) {
        case 0x0A:
            ch = 'A';
            break;
        case 0x0B:
            ch = 'B';
            break;
        case 0x0C:
            ch = 'C';
            break;
        case 0x0D:
            ch = 'D';
            break;
        case '*'://0x0E:
            ch = '*';
            break;
        case '#'://0x0F:
            ch = '#';
            break;
        }
    }

    return ch;
}

static s32 dtmfToDigit_armor(u8 dtmf)
{
    s32 ret = -1;

    if (dtmf >= 0x30 && dtmf <= 0x39)
        ret = dtmf - 0x30;
    else if (dtmf >= 0x41 && dtmf <= 0x44)
        ret = dtmf - 0x37;
    else if (0x2A == dtmf) // '*'
        ret = dtmf;//0x0E;
    else if (0x23 == dtmf) // '#'
        ret = dtmf;//0x0F;
    else
        ret = ERR_BAD_DTMF_CODE;

    return ret;
}


// NOTE: use exactly ADMH_HeaderLen, because ADMH_HeaderLen > AGMH_HeaderLen
static u16 maxMsgBodyLen(void)
{ return PROTOCOL_ARMOR_MAX_MSG_LEN - (ADMH_HeaderLen + 1); }

static u16 SPSMarkerPosition(void)
{ return AGMH_HeaderLen + 2; }

static u16 SPSAuxInfoPosition(void)
{ return AGMH_HeaderLen; }

static u16 SPSACKPosition(void)
{ return AGMH_HeaderLen + 1; }



static u16 qxyz_2_bcd(u8 q, u16 xyz)
{
    u16 ret = (q << 12) |
              ((xyz / 100)      << 8) |
              ((xyz % 100 / 10) << 4) |
              ( xyz % 10);
    OUT_DEBUG_2("qxyz_2_bcd(q=%d,xyz=%d) = %#x\r\n", q, xyz, ret)
    return ret;
}

//static inline u16 dec_2_bcd(u8 dec)
//{ return ((dec / 100) << 8) | ((dec % 100 / 10) << 4) | (dec % 10); }

u16 bcd_2_xyz(u16 bcd)
{ return (((bcd >> 8) & 0x0F) * 100) + (((bcd >> 4) & 0x0F) * 10) + (0x0F & bcd); }

u8 bcd_2_q(u16 bcd)
{ return ((bcd >> 12) & 0x0F); }

/*
 *  compress the message body (each three symbols of the msg body as a byte), cut off CRC
 */
void compressDtmf_armor(u8 *pkt, u16 *realLen)
{
    OUT_DEBUG_2("compressDtmf_armor()\r\n");

    u8 b[PROTOCOL_ARMOR_MAX_MSG_LEN] = {0};
    const u8 nHeaderSymbols = (0 == pkt[ADMH_FrameNum]) ? ADMH_HeaderLen + 4 : ADMH_HeaderLen;
    const u16 targetBodyLen = (*realLen - (nHeaderSymbols + 1)) / 3;

    Ql_memcpy(b, pkt, nHeaderSymbols);

    u8 *iterIn = &pkt[nHeaderSymbols];
    u8 *iterOut = &b[nHeaderSymbols];

    for (u16 i = 0; i < targetBodyLen; ++i) {
        *iterOut = iterIn[0] * 100 + iterIn[1] * 10 + iterIn[2];
        iterIn += 3;
        iterOut += 1;
    }

    const u32 outLen = iterOut - b;

    Ql_memset(pkt, 0, *realLen);
    Ql_memcpy(pkt, b, outLen);
    *realLen = outLen;

// ++++++++++++++++++++++++++++++++++++++++++
Ar_Helper_debugOutDataPacket(pkt, *realLen);
// ++++++++++++++++++++++++++++++++++++++++++
}

/*
 *  decompress the message body (each byte of the msg body as three symbols)
 */
void decompressDtmf_armor(u8 *pkt, u16 *len)
{
    OUT_DEBUG_2("decompressDtmf_armor()\r\n");

    u8 b[PROTOCOL_ARMOR_MAX_MSG_LEN * 3] = {0};
    const u8 nHeaderSymbols = (0 == pkt[ADMH_FrameNum]) ? ADMH_HeaderLen + 4 : ADMH_HeaderLen;
    const u16 srcBodyLen = *len - (nHeaderSymbols + 1);

    Ql_memcpy(b, pkt, nHeaderSymbols);

    u8 *iterIn = &pkt[nHeaderSymbols];
    u8 *iterOut = &b[nHeaderSymbols];

    for (u16 i = 0; i < srcBodyLen; ++i) {
        iterOut[0] = *iterIn / 100;
        iterOut[1] = (*iterIn % 100) / 10;
        iterOut[2] = *iterIn % 10;
        iterIn += 1;
        iterOut += 3;
    }

    *iterOut++ = *iterIn;   // copy CRC value

    const u32 outLen = iterOut - b;

    Ql_memset(pkt, 0, *len);
    Ql_memcpy(pkt, b, outLen);
    *len = outLen;

// ++++++++++++++++++++++++++++++++++++++++++
Ar_Helper_debugOutDataPacket(pkt, *len);
// ++++++++++++++++++++++++++++++++++++++++++
}


// =============================================
// -- DTMF

/*
 *  @return the number of messages actually encoded or an error code
 */
static s32 encode_dtmf(u8 *txData, u8 *txLen)
{
    OUT_DEBUG_2("encode_dtmf(): codec %s\r\n", getCodec_humanize());

    PultMessage m;
    s32 ret = pultTxBuffer.cloneFirst(&m);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    u8 real_len = 0;
    u8 head_len = 0;

    txData[ADMH_ProtocolVer_H] = PROTOCOL_ARMOR_VERSION >> 4;
    txData[ADMH_ProtocolVer_L] = PROTOCOL_ARMOR_VERSION;
    u8 ident = getTxCycleCounter();
    txData[ADMH_Ident_H] = ident / 10;
    txData[ADMH_Ident_L] = ident % 10;
    txData[ADMH_FrameNum] = m.frameNo;
    txData[ADMH_FrameCount] = m.frameTotal;
    txData[ADMH_Qualifier] = m.msg_qualifier;
    txData[ADMH_EventCode_x] = m.msg_code / 100;
    txData[ADMH_EventCode_y] = (m.msg_code % 100) / 10;
    txData[ADMH_EventCode_z] = m.msg_code % 10;

    if (PMQ_AuxInfoMsg == m.msg_qualifier)  // special messages
    {
        const u8 nHeaderSymbols = (0 == m.frameNo) ? ADMH_HeaderLen + 4 : ADMH_HeaderLen;

        head_len = nHeaderSymbols + m.part_len + 1;
        u16 pkt_len = head_len;     // temp u16 argument for decompressDtmf_armor() func

        Ql_memcpy(&txData[nHeaderSymbols], m.complex_msg_part, m.part_len);
        decompressDtmf_armor(txData, &pkt_len);
        real_len = pkt_len;
    }
    else if (PMQ_NewMsgOrSetDisarmed == m.msg_qualifier ||
             PMQ_RestoralOrSetArmed == m.msg_qualifier)   // general messages
    {
        real_len = ADMH_HeaderLen + 10 + 1;
        head_len = ADMH_GeneralMsgLen;

        txData[ADMH_GG_H] = m.group_id / 100;
        txData[ADMH_GG_M] = (m.group_id % 100) / 10;
        txData[ADMH_GG_L] = m.group_id % 10;

        txData[ADMH_CCC_H] = m.identifier / 100;
        txData[ADMH_CCC_M] = (m.identifier % 100) / 10;
        txData[ADMH_CCC_L] = m.identifier % 10;
    }

    txData[ADMH_FrameLen_H] = head_len / 10;
    txData[ADMH_FrameLen_L] = head_len % 10;
    txData[real_len-1] = evaluate_crc_dtmf_armor(txData);

    /* transcode entire message to current DTMF code */
    for (u8 i = 0; i < real_len; ++i)
        txData[i] = digitToDtmf_armor(txData[i]);
    txData[real_len] = 0; // terminate ascii string

    OUT_DEBUG_8("DTMF: \"%s\" , datalen: %d\r\n", txData, real_len);

    *txLen = real_len;
    return 1;
}

static s32 send_dtmf(u8 *txData, u8 txLen)
{
    OUT_DEBUG_2("send_dtmf(): %d symbols\r\n", txLen);
    s32 ret = generateDtmf(txData, txLen);
    start_pult_protocol_timer(ARMOR_TX_MSG_DTMF_TIMEOUT(txLen));
    return ret;
}

static s32 send_dtmf_armor(void)
{
    OUT_DEBUG_2("send_dtmf_armor()\r\n");

    s32 ret = 0;
    u8  len = 0;

    u8 code = getAndClearDtmfCodeToSend_armor();
    if (code) {
        len = 1;
        _pCodec->txBuffer[0] = digitToDtmf_armor(code);
        _pCodec->txBuffer[len] = 0; // terminate ascii string
    }
    else {
        ret = encode_dtmf(_pCodec->txBuffer, &len);
        if (ret <= 0) {
            OUT_DEBUG_1("encode_dtmf(codec=%s) = %d error. Sending aborted.\r\n",
                        getCodec_humanize(), ret);
            return ret;
        }
    }

    ret = send_dtmf(_pCodec->txBuffer, len);
    if (ret < QL_RET_OK) {
        OUT_DEBUG_1("send_dtmf() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}


static s32 recv_dtmf_armor(u8 dtmf)
{
    OUT_DEBUG_2("recv_dtmf_armor()\r\n");
    return dtmfToDigit_armor(dtmf);
}


// =============================================
// -- GPRS

/*
 *  @return the number of messages actually encoded or an error code
 */
static s32 encode_gprs(u8 *txData, u16 *txLen)
{
    OUT_DEBUG_2("encode_gprs(): codec %s\r\n", getCodec_humanize());
    PultMessage m;
    s32 ret = pultTxBuffer.cloneFirst(&m);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    u8 len = 0;
    u8 nMessages = 0;

    txData[AGMH_ProtocolVer] = PROTOCOL_ARMOR_VERSION;
    txData[AGMH_Ident] = getTxCycleCounter();
    txData[AGMH_FrameNum] = m.frameNo;
    txData[AGMH_FrameCount] = m.frameTotal;

    u16 bcd = qxyz_2_bcd(m.msg_qualifier, m.msg_code);
    txData[AGMH_Qualifier_and_X] = bcd >> 8;
    txData[AGMH_Y_and_Z] = bcd;

    /* note: all the aux/info messages are complex by definition */
//    if (PMQ_AuxInfoMsg == m.msg_qualifier)  // special messages
    if (m.bComplex)  // special messages
    {
        u8 nHeaderSymbols = 0;
        u8 *data = NULL;

        // if first frame of complex message
        if(0 == m.frameNo) {
            nHeaderSymbols = AGMH_HeaderLen + 2;
            data = &txData[AGMH_GG];
        }
        else {
            nHeaderSymbols = AGMH_HeaderLen;
            data = &txData[AGMH_GG - 2];
        }

        len = nHeaderSymbols + m.part_len + 1;
        nMessages = 1;

        Ql_memcpy(data, m.complex_msg_part, m.part_len);
    }
    else if (PMQ_NewMsgOrSetDisarmed == m.msg_qualifier ||
             PMQ_RestoralOrSetArmed == m.msg_qualifier)   // general messages
    {
        len = AGMH_GeneralMsgLen;
        nMessages = 1;

        txData[AGMH_GG] = m.group_id;
        txData[AGMH_CCC] = m.identifier;
    }

    txData[AGMH_FrameLen] = len;
    txData[len-1] = evaluate_crc_gprs_armor(txData);

    *txLen = len;
    return nMessages;
}

static void make_gprs_ack(u8 ackId, u8 *txData, u16 *txLen)
{
    OUT_DEBUG_2("make_gprs_ack(): codec %s\r\n", getCodec_humanize());

    const u8 len = AGMH_ACK_Len;

    txData[AGMH_FrameLen] = len;
    txData[AGMH_ProtocolVer] = PROTOCOL_ARMOR_VERSION;
    txData[AGMH_Ident] = ackId;
    txData[AGMH_FrameNum] = 0;      // current 0
    txData[AGMH_FrameCount] = 0;    // from total 0

    u16 bcd = qxyz_2_bcd(PMQ_AuxInfoMsg, PMC_ArmorAck);
    txData[AGMH_Qualifier_and_X] = bcd >> 8;
    txData[AGMH_Y_and_Z] = bcd;

    txData[len-1] = evaluate_crc_gprs_armor(txData);

    *txLen = len;
}

static s32 send_gprs(u8 *txData, u16 txLen)
{
    OUT_DEBUG_2("send_gprs(): %d bytes\r\n", txLen);

    s32 ret = sendByUDP(txData, txLen);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("sendByUDP() = %d error\r\n", ret);
        return ret;
    }
    return RETURN_NO_ERRORS;
}

static s32 send_gprs_armor(void)
{
    OUT_DEBUG_2("send_gprs_armor()\r\n");

    s32 ret = 0;
    u16 txLen = 0;

    u8 ackId = 0;
    bool bSendData = TRUE;
    bool bSendAck = isGprsAckQueued_armor(&ackId, &bSendData);

    if (bSendAck) {
        make_gprs_ack(ackId, _pCodec->txBuffer, &txLen);
        ret = send_gprs(_pCodec->txBuffer, txLen);
        if (ret < RETURN_NO_ERRORS && ret != ERR_SHOULD_NOT_WAIT_REPLY) {
            OUT_DEBUG_1("send_gprs() = %d error\r\n", ret);
            return ret;
        }
    }

    if (bSendData) {   // if send ACK => it's possible to repeat prev pkt after that
        ret = encode_gprs(_pCodec->txBuffer, &txLen);
        if (ret <= 0) {
            OUT_DEBUG_1("encode_gprs(codec=%s) = %d error. Sending aborted.\r\n",
                        getCodec_humanize(), ret);
            return ret;
        }
        ret = send_gprs(_pCodec->txBuffer, txLen);
        if (ret < RETURN_NO_ERRORS && ret != ERR_SHOULD_NOT_WAIT_REPLY) {
            OUT_DEBUG_1("send_gprs() = %d error\r\n", ret);
            return ret;
        }
    }

    start_pult_protocol_timer(ARMOR_COMMON_GPRS_UDP_TIMEOUT);
    return RETURN_NO_ERRORS;
}


//static s32 decode_gprs(u8 *rxData, u16 rxLen)
//{
//    OUT_DEBUG_2("decode_gprs(): codec %s\r\n", getCodec_humanize());

//    u16 qxyz = rxData[AGMH_Qualifier_and_X] << 8 | rxData[AGMH_Y_and_Z];
//    ;

//    return RETURN_NO_ERRORS;
//}

static s32 recv_gprs_armor(u8 *data, s16 len)
{
    OUT_DEBUG_2("recv_gprs_armor()\r\n");
    return RETURN_NO_ERRORS;
}


// =============================================
// -- INIT
void init_codec_armor(PultMessageCodec *codec)
{
    OUT_DEBUG_2("init_codec_armor()\r\n");

    _pCodec = codec;

    codec->type = PMCT_CODEC_A;

    // -- codec's slots
    codec->sends[PCHT_VOICECALL] = &send_dtmf_armor;
    codec->sends[PCHT_GPRS_UDP] = &send_gprs_armor;

    codec->readDtmf = &recv_dtmf_armor;
    codec->readUdp = &recv_gprs_armor;

    codec->maxMsgBodyLen =&maxMsgBodyLen;
    codec->SPSMarkerPosition =&SPSMarkerPosition;
    codec->SPSAuxInfoPosition =&SPSAuxInfoPosition;
    codec->SPSACKPosition =&SPSACKPosition;

    // map_1  -  new msg or set disarmed

    // map_3  -  restore or set armed

}
