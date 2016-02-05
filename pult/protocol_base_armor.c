#include "protocol_base_armor.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "pult/pult_tx_buffer.h"
#include "pult/pult_rx_buffer.h"
#include "pult/connection.h"
#include "pult/voicecall.h"

#include "codecs/codec_armor.h"

#include "core/timers.h"
#include "core/system.h"

#include "service/helper.h"



#define DTMF_STARTING_DELAY                  1000   // ms
#define DTMF_INCOMING_CALL_WAITING_INTERVAL  3000   // ms
#define VOICE_INCOMING_CALL_WAITING_INTERVAL  600000   // ms

#define DTMF_SYMBOL_ASTERISK   '*'
#define DTMF_SYMBOL_SHARP      '#'
#define DTMF_FLAG_MF     DTMF_SYMBOL_SHARP
#define DTMF_FLAG_VMS    DTMF_SYMBOL_ASTERISK
#define DTMF_FLAG_ACK    DTMF_SYMBOL_ASTERISK


typedef enum {
    PPS_Standby,
    PPS_StartingDelay,
    PPS_DataSending,
    PPS_DataReceiving,
    PPS_SessionEnding,
    PPS_VoiceMode,
    PPS_VoiceModeEnding
} PultProtocolState;

typedef void (*PultProtocolStateHandler)(void);


static PultProtocolState _current_state = PPS_Standby;

static u16 _rx_msg_size = 0;
static u16 _received_symbols_qty = 0;

static u8 _received_dtmf_digit = 0;
static u8 _dtmf_digit_to_send = 0;

static bool _timer_out = FALSE;


static void send_MF(void);
static void send_VMS(void);
static void send_ACK(void);

static void process_dtmf_received(u8 digit);
static s32 process_dtmf_rx_pkt(u8 *pkt);


/**************************************
 *  state machine's handlers
 **************************************/
static void standby(void);
static void starting_delay(void);
static void data_sending(void);
static void data_receiving(void);
static void session_ending(void);
static void voice_mode(void);
static void voice_mode_ending(void);

static PultProtocolStateHandler pult_protocol_handlers[] =
{
    &standby,
    &starting_delay,
    &data_sending,
    &data_receiving,
    &session_ending,
    &voice_mode,
    &voice_mode_ending
};


/**************************************
 *  protocol interface
 **************************************/
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
    clear_dtmf_rx_buffer();
    _rx_msg_size = 0;
    _received_symbols_qty = 0;
    _current_state = PPS_DataReceiving;


    if(Ar_System_isAudio_flag()){
        start_pult_protocol_timer(VOICE_INCOMING_CALL_WAITING_INTERVAL);
    }
    else{
        start_pult_protocol_timer(DTMF_INCOMING_CALL_WAITING_INTERVAL);
    }



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


/**************************************
 *  initialize protocol
 **************************************/
void init_pult_protocol_dtmf_armor(PultMessageBuilder *pBuilder)
{
    registerProtocol(PCHT_VOICECALL, &_protocol);
    pBuilder->codec->notifyDtmfDigitReceived = &process_dtmf_received;
}


/**************************************
 *  armor dtmf protocol responses
 **************************************/
void send_MF(void)
{
    OUT_DEBUG_2("send_MF()\r\n");
    _dtmf_digit_to_send = DTMF_FLAG_MF;
    fa_SendMessageToPult();
}

void send_VMS(void)
{
    OUT_DEBUG_2("send_VMS()\r\n");
    _dtmf_digit_to_send = DTMF_FLAG_VMS;
    fa_SendMessageToPult();
}

void send_ACK(void)
{
    OUT_DEBUG_2("send_ACK()\r\n");
    _dtmf_digit_to_send = DTMF_FLAG_ACK;
    fa_SendMessageToPult();
}


u8 getAndClearDtmfCodeToSend_armor(void)
{
    u8 temp = _dtmf_digit_to_send;
    _dtmf_digit_to_send = 0;
    return temp;
}


/**************************************
 * processing
 **************************************/
void process_dtmf_received(u8 digit)
{
    if (PCHT_VOICECALL != getChannel()->type)
        return;

    stop_pult_protocol_timer();
    _received_dtmf_digit = digit;
    pult_protocol_handlers[_current_state]();
}

s32 process_dtmf_rx_pkt(u8 *pkt)
{
    const u8 pktlen = pkt[ADMH_FrameLen_H]*10 + pkt[ADMH_FrameLen_L];

    bool complex = pkt[ADMH_FrameCount] > 0;
    u8 frameNo = pkt[ADMH_FrameNum];
    u8 frameCount = pkt[ADMH_FrameCount];

    OUT_DEBUG_2("process_dtmf_rx_pkt(): frame %d from %d\r\n", frameNo, frameCount);

    if(frameCount > 9 || frameNo > frameCount)
    {
        OUT_DEBUG_1("process_dtmf_rx_pkt(): invalid frameNo = %d  or  frameCount = %d\r\n", frameNo, frameCount);
        return ERR_BAD_FRAMENUM_OR_FRAMECOUNT;
    }

    const bool bFirstFrame = 0 == frameNo;
    u16 datalen = realDtmfMsgLen_armor(pktlen, bFirstFrame);  // evaluate real number of symbols to wait

    compressDtmf_armor(pkt, &datalen); // compress the packet, cut off CRC


    PultMessageRx m = {0};

    m.frameNo = frameNo;
    m.frameTotal = frameCount;
    m.msg_type = PMT_Normal;
    m.account = 0;
    m.bComplex = complex;

    const u8 nHeaderSymbols = bFirstFrame ? ADMH_HeaderLen + 4 : ADMH_HeaderLen;
    m.complex_msg_part = &pkt[nHeaderSymbols];
    m.part_len = datalen - nHeaderSymbols;

    if (bFirstFrame) {
        m.msg_code = (PultMessageCode)(pkt[ADMH_EventCode_x]*100 + pkt[ADMH_EventCode_y]*10 + pkt[ADMH_EventCode_z]);
        m.msg_qualifier = (PultMessageQualifier)pkt[ADMH_Qualifier];
    }

    Ar_PultBufferRx_appendMsgFrame(&m);
    return RETURN_NO_ERRORS;
}


/**************************************
 *  state machine's handlers
 **************************************/
void standby(void)
{
    OUT_DEBUG_8("PPS::standby()\r\n");

    if (_timer_out) {
        _timer_out = FALSE;
        notifyConnectionEstablishingFailed(PCHT_VOICECALL);
        return;
    }

    getAndClearDtmfCodeToSend_armor();

    start_pult_protocol_timer(DTMF_STARTING_DELAY);
    _current_state = PPS_StartingDelay;
}

void starting_delay(void)
{
    OUT_DEBUG_8("PPS::starting_delay()\r\n");

    if (_timer_out)
        _timer_out = FALSE;
    fa_SendMessageToPult();
    _current_state = PPS_DataSending;
}

void data_sending(void)
{
    OUT_DEBUG_8("PPS::data_sending()\r\n");

    if (_timer_out) {
        _timer_out = FALSE;
        notifyProtocolFail(PCHT_VOICECALL);
        _current_state = PPS_Standby;
        return;
    }

    switch (_received_dtmf_digit) {
    case DTMF_FLAG_ACK:
        incrementTxCycleCounter();
        ArmingACK_ArmedLEDSwitchON();
        pultTxBuffer.removeAt(0);
        if (!pultTxBuffer.isEmpty()) {
            fa_SendMessageToPult();
        } else {
            send_MF();
            _current_state = PPS_SessionEnding;
        }
        break;

    case DTMF_FLAG_MF:
//        fa_SendMessageToPult();   NOTE: uncomment for request to repeat prev packet
        break;

    default:
        break;
    }
}

void data_receiving(void)
{
    OUT_DEBUG_8("PPS::data_receiving()\r\n");

    if (_timer_out) {
        OUT_DEBUG_8("_timer_out = TRUE\r\n");
        _timer_out = FALSE;
//        send_MF();                     NOTE: uncomment 2 lines for request to repeat prev packet
//        _current_state = PPS_SessionEnding;
        notifyProtocolEndOk(PCHT_VOICECALL);
        _current_state = PPS_Standby;
        return;
    }

    switch (_received_dtmf_digit) {
    case DTMF_FLAG_VMS:
        if (!pultTxBuffer.isEmpty()) {
            fa_SendMessageToPult();
            _current_state = PPS_DataSending;
        } else {
            send_VMS();
            _current_state = PPS_VoiceMode;
        }
        break;

    case DTMF_FLAG_MF:
        if (!pultTxBuffer.isEmpty()) {
            fa_SendMessageToPult();
            _current_state = PPS_DataSending;
        } else {
            send_MF();
            _current_state = PPS_SessionEnding;
        }
        break;

    default:
        if (0 == _received_symbols_qty)
            clear_dtmf_rx_buffer();
        append_dtmf_digit(_received_dtmf_digit);
        if (++_received_symbols_qty < _rx_msg_size)
            start_pult_protocol_timer(ARMOR_RX_DTMF_TIMEOUT); // wait for the next digit of dtmf pkt

        /* 1st part of the msg size received */
        if (1 == _received_symbols_qty)
        {
            _rx_msg_size = _received_dtmf_digit * 10; // the first (high) digit of the two (of pkt size)
            OUT_DEBUG_7("_rx_msg_size (1) = %d \r\n", _rx_msg_size);
        }
        /* 2nd part of the msg size received */
        else if (2 == _received_symbols_qty)
        {
            _rx_msg_size += _received_dtmf_digit;
            OUT_DEBUG_7("_rx_msg_size (2) = %d \r\n", _rx_msg_size);
        }
        /* adjust real message length after receiving the whole header */
        else if (ADMH_HeaderLen == _received_symbols_qty)
        {
            bool bFirstFrame = 0 == get_dtmf_rx_packet()[ADMH_FrameNum];
            _rx_msg_size = realDtmfMsgLen_armor(_rx_msg_size, bFirstFrame);    // evaluate real number of symbols to wait
            OUT_DEBUG_7("Incoming message size: %d\r\n", _rx_msg_size);
        }
        /* entire message received */
        else if (_rx_msg_size == _received_symbols_qty)
        {
            _received_symbols_qty = 0;
            u8 *pkt = get_dtmf_rx_packet();

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Ar_Helper_debugOutDataPacket(pkt, _rx_msg_size);
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

            if (!check_crc_dtmf_armor(pkt))
                return;

            const u8 current_rx_cycle_counter = pkt[ADMH_Ident_H]*10 + pkt[ADMH_Ident_L];
            const s32 counter_error = checkRxCycleCounterValidity(current_rx_cycle_counter);

            if (RETURN_NO_ERRORS == counter_error) {
                s32 ret = process_dtmf_rx_pkt(pkt);
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("process_dtmf_rx_pkt() = %d error\r\n", ret);
                    return;
                }
                incrementRxCycleCounter();
//                clear_dtmf_rx_buffer();   delete this line if the tests will pass ok
            }
            else if (ERR_PREVIOUS_PKT_INDEX == counter_error) {
                ;   // do not process, only send ACK
            }
            else {
                return;
            }

            send_ACK();
        }
    }

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
if (_received_symbols_qty)
    Ar_Helper_debugOutDataPacket(get_dtmf_rx_packet(), _received_symbols_qty);
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
}

void session_ending(void)
{
    OUT_DEBUG_8("PPS::session_ending()\r\n");

    if (_timer_out) {
        _timer_out = FALSE;
        notifyProtocolEndOk(PCHT_VOICECALL);
        _current_state = PPS_Standby;
        return;
    }

    switch (_received_dtmf_digit) {
    case DTMF_FLAG_ACK:
        break;

    case DTMF_FLAG_MF:
        if (!pultTxBuffer.isEmpty()) {
            fa_SendMessageToPult();
            _current_state = PPS_DataSending;
        } else {
            notifyProtocolEndOk(PCHT_VOICECALL);
            _current_state = PPS_Standby;
        }
        break;

    default:    // if opposite side have any data for me
        clear_dtmf_rx_buffer();
        append_dtmf_digit(_received_dtmf_digit);
        _received_symbols_qty = 1;
        _rx_msg_size = _received_dtmf_digit * 10; // the first (high) digit of the two (of pkt size)
        _current_state = PPS_DataReceiving;
        start_pult_protocol_timer(ARMOR_RX_DTMF_TIMEOUT); // wait for the next digit of dtmf pkt
    }
}

void voice_mode(void)
{
    OUT_DEBUG_8("PPS::voice_mode()\r\n");

    if (_timer_out) {
        _timer_out = FALSE;

        return;
    }

}

void voice_mode_ending(void)
{
    OUT_DEBUG_8("PPS::voice_mode_ending()\r\n");

    if (_timer_out) {
        _timer_out = FALSE;

        return;
    }

}
