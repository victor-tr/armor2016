//#include "codecs.h"
//#include "codec_lun_9t.h"

//#include "common/debug_macro.h"

//#include "db/db.h"

//#include "pult/gprs_udp.h"
//#include "pult/pult_tx_buffer.h"

//#include "service/humanize.h"


//// WARNING: what to do with unsolicited messages when using DTMF ??? should I just delete it from the fifo ???

//static u8 lastSent_EventHeader_gprs = 0x2F;     // can be 0x2F, 0x30...0x37
//static bool bNeedPultResponse = TRUE;
//static PultMessageCodec *_codec = NULL;

//static u16 _last_sent_messages_qty = 0;


//// =============================================
//// -- HELPERS
//static inline void add_crc_dtmf_lun9t(u8 *buffer)
//{
//    u32 temp = buffer[0] + buffer[1] + buffer[2];
//    u8 ret = 0;

//    if (temp % 15)
//        ret = (temp/15 + 1) * 15;
//    else
//        ret = temp;

//    ret -= temp;

//    if (! ret)
//        ret = 0x0F;

//    buffer[3] = ret;
//}

//static inline u8 adjust_code_by_id(u8 code, u16 identifier)
//{
//    switch (code) {
//    // TODO: add all codes related to Zone_ID or User_ID
//    case 0x70:
//    case 0x80:
//    case 0x90:
//    case 0xB0:
//        if (identifier < 1 || identifier > 8) {      // lun-9t has only 8 protection loops (1..8)
//            OUT_DEBUG_1("Zone/User ID has invalid value.\r\n");
//            return 0;
//        }

//        code += identifier;
//        break;
//    }

//    return code;
//}

//static void processPultMessageBuffer(s32 result)
//{
//    if (ERR_UNSOLICITED_QUERY == result)    // don't process if unsolicited data received
//        return;

//    const PultChannel *ch = getChannel();

//    if (_last_sent_messages_qty > 0)
//    {
//        s32 ret = pultTxBuffer.removeFirstN(_last_sent_messages_qty);
//        if (ret < RETURN_NO_ERRORS)
//            OUT_DEBUG_1("pultTxBuffer::removeFirstN() = %d error\r\n", ret);

//        _last_sent_messages_qty = 0;
//        OUT_DEBUG_3("%d message(s) remain in the FIFO buffer\r\n", pultTxBuffer.size());

//        /* should clear the attempts counter if connection was
//           successfully established and the message(s) was sent OK */
//        resetChannelAttemptsCounter();
//    }

//    if (!pultTxBuffer.isEmpty())
//    {
//        if (PCHT_VOICECALL == ch->type) setPultSessionState(PSS_ESTABLISHING);
//        fa_SetupConnection();
//    }
//    else
//    {
//        setPultSessionState(PSS_CLOSED);
//    }
//}

//static char digitToDtmf_lun9t(u8 digit)
//{
//    char out = 0;

//    switch (digit) {
//    case 0x00:
//        out = 'D';
//        break;
//    case 0x01:
//        out = '1';
//        break;
//    case 0x02:
//        out = '2';
//        break;
//    case 0x03:
//        out = '3';
//        break;
//    case 0x04:
//        out = '4';
//        break;
//    case 0x05:
//        out = '5';
//        break;
//    case 0x06:
//        out = '6';
//        break;
//    case 0x07:
//        out = '7';
//        break;
//    case 0x08:
//        out = '8';
//        break;
//    case 0x09:
//        out = '9';
//        break;
//    case 0x0A:
//        out = '0';
//        break;
//    case 0x0B:
//        out = '*';
//        break;
//    case 0x0C:
//        out = '#';
//        break;
//    case 0x0D:
//        out = 'A';
//        break;
//    case 0x0E:
//        out = 'B';
//        break;
//    case 0x0F:
//        out = 'C';
//        break;
//    }

//    return out;
//}

//static s8 dtmfToDigit_lun9t(u8 dtmf)
//{
//    return 0;
//}


//// =============================================
//// -- DTMF

///*
// *  Returns the number of messages actually encoded or error code
// */
//static s32 encode_dtmf(u8 *txData, u16 *txLen)
//{
//    OUT_DEBUG_2("encode_dtmf(): codec %s\r\n", getCodec_humanize());

//    lastSent_EventHeader_gprs = 0x2F;   // reset gprs-header if gprs channels were failed
//    u8 tempcode = 0;

//    PultMessage m;
//    s32 ret = pultTxBuffer.cloneFirst(&m);
//    if (ERR_BUFFER_IS_EMPTY == ret)
//        return RETURN_NO_ERRORS;

//    switch (m.msg_qualifier) {
//    case PMQ_NewMsgOrSetDisarmed:
//        tempcode = (u8)_codec->enc_map_1[m.msg_code];
//        break;
//    case PMQ_RestoralOrSetArmed:
//        tempcode = (u8)_codec->enc_map_3[m.msg_code];
//        break;
//    case PMQ_RepeatPreviouslySentMsg:
//        break;
//    }

//    u8 code = adjust_code_by_id(tempcode, m.identifier);

//    if (0 == code)
//        return ERR_CANNOT_ENCODE_MSG;

//    txData[0] = code & 0x0F;
//    txData[1] = code >> 4;
//    txData[2] = m.group_id;
//    add_crc_dtmf_lun9t(txData);

//    OUT_DEBUG_8("HEX:  %#x %#x %#x %#x\r\n", txData[0], txData[1], txData[2], txData[3]);

//    const u8 len = 4;

//    /* transcode entire message to current DTMF code */
//    for (u8 i = 0; i < len; ++i)
//        txData[i] = digitToDtmf_lun9t(txData[i]);

//    txData[len] = 0;
//    OUT_DEBUG_8("DTMF: %s\r\n", txData);

////    const char *append = ",350,350";          it's for ARMOR codec !!! optional !!!
////    Ql_strcpy((char *)&out[len], append);

//    *txLen = len;
//    return 1;
//}

//static s32 send_dtmf(u8 *txData, u16 txLen)
//{
//    OUT_DEBUG_2("send_dtmf(): %s\r\n", txData);

//    s32 ret = Ql_VTS(txData, txLen);
//    if (ret < RETURN_NO_ERRORS)
//        return ret;
//    else
//        return ERR_SHOULD_NOT_WAIT_REPLY;
//}

//static s32 recvViaDTMF(u8 **rxData, u16 *rxLen)
//{
//    OUT_DEBUG_2("recvViaDTMF()\r\n");
//    return RETURN_NO_ERRORS;
//}

//static s32 decode_dtmf(u8 *rxData, u16 rxLen)
//{
//    OUT_DEBUG_2("decode_dtmf(): codec %s\r\n", getCodec_humanize());
//    return RETURN_NO_ERRORS;
//}


//static s32 send_dtmf_lun9t(void)
//{
//    OUT_DEBUG_2("send_dtmf_lun9t()\r\n");

//    s32 ret = 0;
//    u16 txLen = 0;

//    ret = encode_dtmf(_codec->txBuffer, &txLen);
//    if (ret <= 0) {
//        OUT_DEBUG_1("encode_dtmf(codec=%s) = %d error. The message won't be sent.\r\n",
//                    getCodec_humanize(), ret);
//        _last_sent_messages_qty = 0;
//        return ret;
//    }

//    _last_sent_messages_qty = ret;

//    ret = send_dtmf(_codec->txBuffer, txLen);
//    if (ret < RETURN_NO_ERRORS && ret != ERR_SHOULD_NOT_WAIT_REPLY) {
//        OUT_DEBUG_1("send_dtmf() = %d error\r\n", ret);
//        return ret;
//    }

//    if (ERR_SHOULD_NOT_WAIT_REPLY == ret) processPultMessageBuffer(ret);
//    return RETURN_NO_ERRORS;
//}

//static s32 recv_dtmf_lun9t()
//{
//    OUT_DEBUG_2("recv_dtmf_lun9t()\r\n");

//    s32 ret = 0;
//    u16 rxLen = 0;
//    u8 *rxBuffer = 0;

//    ret = recvViaDTMF(&rxBuffer, &rxLen);
//    if (ret < RETURN_NO_ERRORS) {
//        OUT_DEBUG_1("recvViaDTMF(codec=%s) = %d error\r\n", getCodec_humanize(), ret);
//    } else {
//        ret = decode_dtmf(rxBuffer, rxLen);
//        if (ret < RETURN_NO_ERRORS && ret != ERR_UNSOLICITED_QUERY) {
//            OUT_DEBUG_1("decode_dtmf(codec=%s) = %d error.\r\n", getCodec_humanize(), ret);
//        }
//    }

//    if (ERR_UNSOLICITED_QUERY == ret)
//        ;   // reserved
//    else if (ret < RETURN_NO_ERRORS)
//        return ret;
//    else
//        processPultMessageBuffer(ret);

//    return RETURN_NO_ERRORS;
//}

//// =============================================
//// -- GPRS

///*
// *  Returns the number of messages actually encoded or error code
// */
//static s32 encode_gprs(u8 *txData, u16 *txLen)
//{
//    OUT_DEBUG_2("encode_gprs(): codec %s\r\n", getCodec_humanize());

//    txData[0] = lastSent_EventHeader_gprs;

//    u16 outpos = 1;
//    u16 msgpos = 0;

//    while (msgpos < pultTxBuffer.size()) {
//        PultMessage m;
//        s32 ret = pultTxBuffer.cloneAt(msgpos++, &m);
//        if (ERR_BUFFER_IS_EMPTY == ret)
//            return 0;

//        // -- special encoding procedure for service messages
//        switch (m.msg_code) {
//        case PMC_ManualTriggerTestReport:
//            if (msgpos > 1)
//                return msgpos - 1;

//            Ql_memset(txData, 0, 32);
//            txData[0] =  'q';  // 0x71
//            txData[3] = 0x02;
//            txData[4] = 0xFF;
//            txData[5] = 0xFF;
//            txData[6] = 0x01;
//            txData[7] = 0x50;
//            txData[8] = 0x04;
//            txData[9] = 0xFF;
//            *txLen = 32;
//            bNeedPultResponse = FALSE;
//            return msgpos;

//        case PMC_PeriodicTestReport:
//            if (msgpos > 1)
//                return msgpos - 1;

//            Ql_memset(txData, 0, 32);
//            txData[0] =  'L';  // 0x4C
//            *txLen = 32;
//            bNeedPultResponse = FALSE;
//            return msgpos;

//        default:
//            break;
//        }
//        // -- end

//        u8 tempcode = 0;

//        switch (m.msg_qualifier) {
//        case PMQ_NewMsgOrSetDisarmed:
//        {
//            tempcode = (u8)_codec->enc_map_1[m.msg_code];
//            break;
//        }
//        case PMQ_RestoralOrSetArmed:
//        {
//            tempcode = (u8)_codec->enc_map_3[m.msg_code];
//            break;
//        }
//        case PMQ_RepeatPreviouslySentMsg:
//            break;
//        }

//        u8 code = adjust_code_by_id(tempcode, m.identifier);

//        if (0 == code)
//            return ERR_CANNOT_ENCODE_MSG;

//        txData[outpos++] = code;
//        txData[outpos++] = 0x00;

//        *txLen = outpos;
//        bNeedPultResponse = TRUE;
//    }

//    return msgpos;
//}

//static s32 send_gprs(u8 *txData, u16 txLen)
//{
//    OUT_DEBUG_2("send_gprs()\r\n");

//    s32 ret = sendByUDP(txData, txLen);

//    if (ret > 0) {
//        if (bNeedPultResponse) {
//            getGprsPeer()->datalen = 0;   // clear before wait a pult response
//            start_pult_protocol_timer(WAIT_FOR_PULT_RESPONSE_INTERVAL);
//        } else {
//            return ERR_SHOULD_NOT_WAIT_REPLY;
//        }
//    }

//    return ret;
//}

//static s32 recv_gprs(u8 **rxData, u16 *rxLen)
//{
//    OUT_DEBUG_2("recv_gprs()\r\n");

//    if (!getGprsPeer()->datalen)
//        return ERR_READ_INCOMING_MSG_FAILED;

//    *rxLen = getGprsPeer()->datalen;
//    *rxData = getGprsPeer()->rx_datagram;

//    return RETURN_NO_ERRORS;
//}

//static s32 decode_gprs(u8 *rxData, u16 rxLen)
//{
//    OUT_DEBUG_2("decode_gprs(): codec %s\r\n", getCodec_humanize());

//    switch ((char)rxData[0]) {
//    case 'Q':
//    {
//        PultMessage m;
//        m.msg_code      = PMC_ManualTriggerTestReport;
//        reportToPult()
//        return ERR_UNSOLICITED_QUERY;
//    }

//    case 'L':
//    {
//        PultMessage m;
//        m.msg_code      = PMC_PeriodicTestReport;
//        reportToPult()
//        return ERR_UNSOLICITED_QUERY;
//    }

//    case 'O':
//        switch ((char)rxData[1]) {
//        case '/':
//        case '0':
//        case '1':
//        case '2':
//        case '3':
//        case '4':
//        case '5':
//        case '6':
//        case '7':
//            break;

//        default:
//            return ERR_CANNOT_DECODE_MSG;
//        }

//        lastSent_EventHeader_gprs = lastSent_EventHeader_gprs >= 0x37
//                                        ? 0x30
//                                        : ++lastSent_EventHeader_gprs;
//        break;

//    default:
//        return ERR_CANNOT_DECODE_MSG;
//    }

//    return RETURN_NO_ERRORS;
//}


//static s32 send_gprs_lun9t(void)
//{
//    OUT_DEBUG_2("send_gprs_lun9t()\r\n");

//    s32 ret = 0;
//    u16 txLen = 0;

//    ret = encode_gprs(_codec->txBuffer, &txLen);
//    if (ret <= 0) {  // zero messages amount is error too
//        OUT_DEBUG_1("encode_gprs(codec=%s) = %d error. The message won't be sent.\r\n",
//                    getCodec_humanize(), ret);
//        _last_sent_messages_qty = 0;
//        return ret;
//    }

//    _last_sent_messages_qty = ret;

//    ret = send_gprs(_codec->txBuffer, txLen);
//    if (ret < RETURN_NO_ERRORS && ret != ERR_SHOULD_NOT_WAIT_REPLY) {
//        OUT_DEBUG_1("send_gprs() = %d error\r\n", ret);
//        return ret;
//    }

//    if (ERR_SHOULD_NOT_WAIT_REPLY == ret) processPultMessageBuffer(ret);
//    return RETURN_NO_ERRORS;
//}

//static s32 recv_gprs_lun9t()
//{
//    OUT_DEBUG_2("recv_gprs_lun9t()\r\n");

//    s32 ret = 0;
//    u16 rxLen = 0;
//    u8 *rxBuffer = 0;

//    ret = recv_gprs(&rxBuffer, &rxLen);
//    if (ret < RETURN_NO_ERRORS) {
//        OUT_DEBUG_1("recv_gprs(codec=%s) = %d error\r\n", getCodec_humanize(), ret);
//    } else {
//        ret = decode_gprs(rxBuffer, rxLen);
//        if (ret < RETURN_NO_ERRORS && ret != ERR_UNSOLICITED_QUERY) {
//            OUT_DEBUG_1("decode_gprs(codec=%s) = %d error.\r\n", getCodec_humanize(), ret);
//        }
//    }

//    if (ERR_UNSOLICITED_QUERY == ret)
//        ;   // reserved
//    else if (ret < RETURN_NO_ERRORS)
//        return ret;
//    else
//        processPultMessageBuffer(ret);

//    return RETURN_NO_ERRORS;
//}



//// =============================================
//// -- INIT
//void init_codec_lun9t(PultMessageCodec *codec)
//{
//    OUT_DEBUG_2("init_codec_lun9t()\r\n");

//    _codec = codec;

//    codec->type = PMCT_CODEC_LT9;

//    // -- codec's slots
//    codec->sends[PCHT_VOICECALL] = &send_dtmf_lun9t;
//    codec->sends[PCHT_GPRS_UDP] = &send_gprs_lun9t;

//    codec->reads[PCHT_VOICECALL] = &recv_dtmf_lun9t;
//    codec->reads[PCHT_GPRS_UDP] = &recv_gprs_lun9t;

//    // map_1  -  new msg or set disarmed
//    codec->enc_map_1[PMC_] = 0xB0;
//    codec->enc_map_1[PMC_] = 0x80;
//    codec->enc_map_1[PMC_] = 0x80;

//}
