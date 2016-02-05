//#include "pult/pult_rx_prebuffer_armor.h"
//#include "pult/pult_rx_buffer.h"

//#include "codecs/codec_armor.h"


//static s16 _rx_msg_len = 0;

//static void build_rx_pult_message_callback(void);
//static void msg_cleanup_callback(PultMessageRxPart *msg);

//REGISTER_FIFO_C(PultMessageRxPart, pultRxPrebuffer, &build_rx_pult_message_callback, &msg_cleanup_callback)


//void build_rx_pult_message_callback(void)
//{
//    if (pTail->d.m->frameNo != pTail->d.m->frameTotal)
//        return;

//    PultMessageRx bigmsg = {0};
//    PultMessage *m = pHead->d.m;

//    bigmsg.bComplex = m->bComplex;

//    bigmsg.frameNo = 0;
//    bigmsg.frameTotal = m->frameTotal;

//    bigmsg.group_id = m->group_id;
//    bigmsg.identifier = m->identifier;
//    bigmsg.msg_code = m->msg_code;
//    bigmsg.msg_qualifier = m->msg_qualifier;
//    bigmsg.msg_type = m->msg_type;
//    bigmsg.account = m->account;

//    bigmsg.part_len = m->part_len;

//    if (m->bComplex) {
//        bigmsg.complex_msg_part = Ql_MEM_Alloc(_rx_msg_len);
//        if (!bigmsg.complex_msg_part) {
//            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", _rx_msg_len);
//            return;
//        }
//        while (pHead->pNext) {
//            ;
//        }
//    }

//    s32 ret = process_dtmf_rx_pkt(&bigmsg);
//    if (ret < RETURN_NO_ERRORS)
//        OUT_DEBUG_1("process_dtmf_rx_pkt() = %d error\r\n", ret)
//}

//void msg_cleanup_callback(PultMessageRxPart *msg)
//{
//    if (msg && msg->m->bComplex && msg->m->complex_msg_part)
//        Ql_MEM_Free(msg->m->complex_msg_part);
//    Ql_MEM_Free(msg->m);
//}


//s32 append_to_pult_rx_prebuffer_dtmf_armor(u8 *pkt)
//{
//    OUT_DEBUG_2("append_to_pult_rx_prebuffer_dtmf_armor()\r\n");

//    u8 pktlen = pkt[ADMH_FrameLen_H]*10 + pkt[ADMH_FrameLen_L];
//    u8 datalen = pktlen - (ADMH_HeaderLen + 1);

//    bool complex = pkt[ADMH_FrameCount] > 0;
//    s32 ret = 0;

//    if (complex) {
//        _rx_msg_len += datalen;

//        PultMessage *m = Ql_MEM_Alloc(sizeof(PultMessage));
//        if (!m) {
//            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", sizeof(PultMessage));
//            return ERR_GET_NEW_MEMORY_FAILED;
//        }

//        m->bComplex = complex;
//        m->part_len = datalen;

//        m->complex_msg_part = Ql_MEM_Alloc(datalen);
//        if (!m->complex_msg_part) {
//            Ql_MEM_Free(m);
//            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", datalen);
//            return ERR_GET_NEW_MEMORY_FAILED;
//        }

//        m->frameNo = pkt[ADMH_FrameNum];
//        m->frameTotal = pkt[ADMH_FrameCount];

//        // -- if previous message was failed, cleanup the buffer before building new message
//        if (m->frameNo <= pTail->d.m->frameNo) pultRxPrebuffer.clear();

//        m->group_id = 0;
//        m->identifier = 0;
//        m->msg_code = pkt[ADMH_EventCode_x]*100 + pkt[ADMH_EventCode_y]*10 + pkt[ADMH_EventCode_z];
//        m->msg_qualifier = (PultMessageQualifier)pkt[ADMH_Qualifier];
//        m->msg_type = PMT_Normal;
//        m->account = 0;

//        PultMessageRxPart part = {m};
//        ret = pultRxPrebuffer.enqueue(&part);
//        if (ret < RETURN_NO_ERRORS) {
//            msg_cleanup_callback(&part);
//            OUT_DEBUG_1("pultRxPrebuffer::enqueue() = %d error\r\n", ret);
//        }
//    }
//    else {
//        _rx_msg_len = 0;

//        PultMessage m = {0};

//        m.bComplex = ;
//        m.part_len = ;
//        m.complex_msg_part = ;

//        m.frameNo = ;
//        m.frameTotal = ;

//        m.group_id = ;
//        m.identifier = ;
//        m.msg_code = ;
//        m.msg_qualifier = ;
//        m.msg_type = ;
//        m.account = ;

//        ret = process_dtmf_rx_pkt(&m);
//    }

//    return ret;
//}

//s32 append_to_pult_rx_prebuffer_gprs_armor(u8 *pkt)
//{
//    OUT_DEBUG_2("append_to_pult_rx_prebuffer_gprs_armor()\r\n");

//    //rx_msg_len += ;

////    PultMessage *m = Ql_MEM_Alloc(sizeof(PultMessage));

////    m->bComplex = ;
////    m->part_len = ;
////    m->complex_msg_part = ;

////    m->frameNo = ;
////    m->frameTotal = ;

////    m->group_id = ;
////    m->identifier = ;
////    m->msg_code = ;
////    m->msg_qualifier = ;
////    m->msg_type = ;
////    m->account = ;

//    return RETURN_NO_ERRORS;
//}
