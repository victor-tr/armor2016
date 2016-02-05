#include "protocol_gprs_armor.h"

#include "pult/pult_tx_buffer.h"
#include "pult/pult_rx_buffer.h"
#include "pult/connection.h"
#include "pult/gprs_udp.h"

#include "codecs/codec_armor.h"

#include "core/timers.h"


typedef enum {
    PPS_Standby,
    PPS_DataExchange
} PultProtocolState;

typedef void (*PultProtocolStateHandler)(void);


static bool _bAckQueued = FALSE;
static bool _bWaitForAck = FALSE;
static u8 _rx_pkt_id = 0;

static PultProtocolState _current_state = PPS_Standby;
static bool _timer_out = FALSE;

static u8 *_rx_datagram = NULL;
static u16 _rx_datagram_len = 0;


static void send_ACK(u8 rxPktId);
static void process_udp_datagram_received(u8 *data, u16 len);
static s32 process_gprs_rx_pkt(u8 *pkt);


//**************************************
//  state machine's handlers
//**************************************
static void standby(void);
static void data_exchange(void);

static PultProtocolStateHandler pult_protocol_handlers[] =
{
    &standby,
    &data_exchange
};


//**************************************
//*  protocol interface
//**************************************
static s32 startSendProtocol(void);
static s32 startRecvProtocol(void);
static s32 stopProtocol(void);
static bool isProtocolActive(void);
static void pultProtocol_timerHandler(void *data);

static IProtocol _protocol =
{
    &startSendProtocol,
    &startRecvProtocol,
    &stopProtocol,
    &isProtocolActive,
    &pultProtocol_timerHandler
};


s32 startSendProtocol(void)
{
    standby();  // call the handler of 'standby' state
    return RETURN_NO_ERRORS;
}

s32 startRecvProtocol(void)
{
    _current_state = PPS_DataExchange;
    return RETURN_NO_ERRORS;
}

s32 stopProtocol(void)
{
    stop_pult_protocol_timer();
    _current_state = PPS_Standby;
    return RETURN_NO_ERRORS;
}

bool isProtocolActive(void)
{ return PPS_Standby != _current_state; }

void pultProtocol_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_PultProtocol_Timer));
    _timer_out = TRUE;
    pult_protocol_handlers[_current_state]();
}


//**************************************
//  initialize protocol
//**************************************
void init_pult_protocol_udp_armor(PultMessageBuilder *pBuilder)
{
    registerProtocol(PCHT_GPRS_UDP, &_protocol);
    pBuilder->codec->notifyUdpDatagramReceived = &process_udp_datagram_received;
}


//**************************************
//  armor gprs protocol responses
//**************************************
void send_ACK(u8 rxPktId)
{
    OUT_DEBUG_2("send_ACK(id=%d)\r\n", rxPktId);
    _bAckQueued = TRUE;
    _rx_pkt_id = rxPktId;
    fa_SendMessageToPult();
}


bool isGprsAckQueued_armor(u8 *ackId, bool *repeatSending)
{
    bool temp = _bAckQueued;
    if (_bAckQueued) {
        _bAckQueued = FALSE;
        *ackId = _rx_pkt_id;
        *repeatSending = _bWaitForAck;
    }
    else {
        *repeatSending = TRUE;
    }
    return temp;
}


/**************************************
 * processing
 **************************************/
void process_udp_datagram_received(u8 *data, u16 len)
{
    /* prepare the datagram to processing */
    if (!check_crc_gprs_armor(data))
    {
        // NOTE: If you had bad data - update process
        if (!pultTxBuffer.isEmpty()) {
            _bWaitForAck = TRUE;
            fa_SendMessageToPult();
        } else {
            start_pult_protocol_timer(5000);
        }
        return;
    }

    u16 qxyz = data[AGMH_Qualifier_and_X] << 8 | data[AGMH_Y_and_Z];
    u16 xyz = bcd_2_xyz(qxyz);
    u8 q = bcd_2_q(qxyz);

    /* if ACK - increment txCycleCounter and send next message */
    if (PMC_ArmorAck == xyz && PMQ_AuxInfoMsg == q)
    {
        /* process only if received ACK for last sent packet */
        if (getTxCycleCounter() == data[AGMH_Ident])
        {
            stop_pult_protocol_timer();
            _bWaitForAck = FALSE;
            incrementTxCycleCounter();
            ArmingACK_ArmedLEDSwitchON();
            pultTxBuffer.removeAt(0);
            if (!pultTxBuffer.isEmpty()) {
                _bWaitForAck = TRUE;
                fa_SendMessageToPult();
            } else {
                start_pult_protocol_timer(5000);
            }
        }
        return;
    }

    /* if data - process it */
    _rx_datagram = data;
    _rx_datagram_len = len;
    pult_protocol_handlers[_current_state]();
}


s32 process_gprs_rx_pkt(u8 *pkt)
{
    const u8 pktlen = pkt[AGMH_FrameLen];

    bool complex = pkt[AGMH_FrameCount] > 0;
    u8 frameNo = pkt[AGMH_FrameNum];
    u8 frameCount = pkt[AGMH_FrameCount];

    OUT_DEBUG_2("process_gprs_rx_pkt()\r\n");

    if(frameCount > 9 || frameNo > frameCount)
    {
        OUT_DEBUG_1("process_gprs_rx_pkt(): invalid frameNo = %d  or  frameCount = %d\r\n", frameNo, frameCount);
        return ERR_BAD_FRAMENUM_OR_FRAMECOUNT;
    }

    const bool bFirstFrame = 0 == frameNo;

    PultMessageRx m = {0};

    m.frameNo = frameNo;
    m.frameTotal = frameCount;
    m.msg_type = PMT_Normal;
    m.account = 0;
    m.bComplex = complex;

    const u8 nHeaderSymbols = bFirstFrame ? AGMH_HeaderLen + 2 : AGMH_HeaderLen;
    m.complex_msg_part = &pkt[nHeaderSymbols];
    m.part_len = pktlen - (nHeaderSymbols + 1);


    if (bFirstFrame) {
        const u16 bcd = pkt[AGMH_Qualifier_and_X] << 8 | pkt[AGMH_Y_and_Z];
        m.msg_code = (PultMessageCode)bcd_2_xyz(bcd);
        m.msg_qualifier = (PultMessageQualifier)bcd_2_q(bcd);
    }

    Ar_PultBufferRx_appendMsgFrame(&m);
    return RETURN_NO_ERRORS;
}


//**************************************
//  state machine's handlers
//**************************************
void standby(void)
{
    OUT_DEBUG_8("PPS::standby()\r\n");

    if (_timer_out) {
        _timer_out = FALSE;
        notifyProtocolFail(PCHT_GPRS_UDP);
        return;
    }

    _bAckQueued = FALSE;
    _bWaitForAck = TRUE;
    fa_SendMessageToPult();
    _current_state = PPS_DataExchange;
}

void data_exchange(void)
{
    OUT_DEBUG_8("PPS::data_exchange()\r\n");

    if (_timer_out) {
        OUT_DEBUG_8("_timer_out=TRUE\r\n");
        _timer_out = FALSE;
        if (pultTxBuffer.isEmpty()) {
            OUT_DEBUG_8("isEmpty\r\n");
            notifyProtocolEndOk(PCHT_GPRS_UDP);
            _current_state = PPS_Standby;
        }
        else {
            if (_bWaitForAck) {
                OUT_DEBUG_8("_bWaitForAck=TRUE\r\n");
                notifyProtocolFail(PCHT_GPRS_UDP);
                _current_state = PPS_Standby;
            }
            else {
                OUT_DEBUG_8("_bWaitForAck=FALSE\r\n");
                _bWaitForAck = TRUE;
                fa_SendMessageToPult();
            }
        }
        OUT_DEBUG_8("TIMER PSS STOPED");
        return;
    }

    OUT_DEBUG_8("_timer_out=FALSE\r\n");

    const u8 current_rx_cycle_counter = _rx_datagram[AGMH_Ident];
    const s32 counter_error = checkRxCycleCounterValidity(current_rx_cycle_counter);

    if (RETURN_NO_ERRORS == counter_error)
    {
        send_ACK(current_rx_cycle_counter);

        s32 ret = process_gprs_rx_pkt(_rx_datagram);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("process_gprs_rx_pkt() = %d error\r\n", ret);
        }
        incrementRxCycleCounter();
    }
    else if (ERR_PREVIOUS_PKT_INDEX == counter_error)
    {
        send_ACK(current_rx_cycle_counter);   // do not process, only send ACK
    }
    else{
        OUT_DEBUG_8("Go Go Timer \r\n");
        fa_SendMessageToPult();
    }
}
