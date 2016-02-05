#include "voicecall.h"
#include "gprs_udp.h"
#include "sms.h"

#include "pult/pult_tx_buffer.h"
#include "pult/pult_rx_buffer.h"
#include "pult/connection.h"
#include "pult/connection_voicecall.h"
#include "pult/connection_udp.h"
#include "pult/connection_sms.h"

#include "states/armingstates.h"

#include "common/errors.h"
#include "common/debug_macro.h"

#include "codecs/codecs.h"

#include "service/humanize.h"

#include "core/system.h"
#include "core/simcard.h"
#include "core/atcommand.h"
#include "core/timers.h"


static void resetPultChannel(void);
static void setPultSessionState(PultSessionState state);
static void fa_SetupConnection(void);

static u16 ps_waiting_activate_iteration_counter = 0;


static PultChannel  _pult_channel;
static IConnection *_connections[PCHT_MAX] = {0};


// ============================================================================
// --  IConnection
// ============================================================================
static s32 establishConnection(void)
{
#ifndef NO_SEND_MSG
    if (_connections[_pult_channel.type]->establish)
        return _connections[_pult_channel.type]->establish();
    else
        return ERR_OPERATION_NOT_IMPLEMENTED;
#else
    pultTxBuffer.clear();
    setPultSessionState(PSS_CLOSED);
    return ERR_SHOULD_STOP_OPERATION;
#endif // !NO_SEND_MSG
}

static s32 closeConnection(void)
{
    if (_connections[_pult_channel.type]->close)
        return _connections[_pult_channel.type]->close();
    else
        return ERR_OPERATION_NOT_IMPLEMENTED;
}

static s32 trackConnectionEstablishing(void)
{
    if (_connections[_pult_channel.type]->trackEstablishing)
        return _connections[_pult_channel.type]->trackEstablishing();
    else
        return ERR_OPERATION_NOT_IMPLEMENTED;
}

static bool canCurrentChannelTypeBeUsed(void)
{
    if (_connections[_pult_channel.type]->canCurrentChannelTypeBeUsed)
        return _connections[_pult_channel.type]->canCurrentChannelTypeBeUsed();
    else
        return FALSE;
}

static bool isLastAttemptForChannelItem(void)
{
    if (_connections[_pult_channel.type]->isLastAttemptForChannelItem)
        return _connections[_pult_channel.type]->isLastAttemptForChannelItem();
    else
        return TRUE;
}

static bool isLastChannelItemForType(void)
{
    if (_connections[_pult_channel.type]->isLastChannelItemForType)
        return _connections[_pult_channel.type]->isLastChannelItemForType();
    else
        return TRUE;
}

static bool canCurrentChannelItemBeUsed(void)
{
    if (_connections[_pult_channel.type]->canCurrentChannelItemBeUsed)
        return _connections[_pult_channel.type]->canCurrentChannelItemBeUsed();
    else
        return FALSE;
}
// ============================================================================


// ============================================================================
// --  IProtocol
// ============================================================================
static s32 startSendProtocol(void)
{
    IProtocol *protocol = getProtocol(_pult_channel.type);
    if (protocol && protocol->startSendProtocol)
        return protocol->startSendProtocol();
    else
        return ERR_OPERATION_NOT_IMPLEMENTED;
}

static s32 startRecvProtocol(void)
{
    IProtocol *protocol = getProtocol(_pult_channel.type);
    if (protocol && protocol->startRecvProtocol)
        return protocol->startRecvProtocol();
    else
        return ERR_OPERATION_NOT_IMPLEMENTED;
}

static s32 stopProtocol(void)
{
    IProtocol *protocol = getProtocol(_pult_channel.type);
    if (protocol && protocol->stopProtocol)
        return protocol->stopProtocol();
    else
        return ERR_OPERATION_NOT_IMPLEMENTED;
}

static bool isProtocolActive(void)
{
    IProtocol *protocol = getProtocol(_pult_channel.type);
    if (protocol && protocol->isActive)
        return protocol->isActive();
    else
        return TRUE;    // => so the channel will be switched
}

// ----------------------------------------------------------------------------
void notifyProtocolFail(PultChannelType type)
{
    if (_pult_channel.type != type)
        return;
    closeConnection();
    setPultSessionState(PSS_FAILED);
    fa_SetupConnection();
}

void notifyProtocolEndOk(PultChannelType type)
{
    if (_pult_channel.type != type)
        return;
    closeConnection();
    setPultSessionState(PSS_CLOSED);
}

s32 start_pult_protocol_timer(u32 milliseconds)
{ return Ar_Timer_startSingle(TMR_PultProtocol_Timer, milliseconds); }

s16 stop_pult_protocol_timer(void)
{ return Ar_Timer_stop(TMR_PultProtocol_Timer); }
// ============================================================================


// ============================================================================
// --
// ============================================================================
void notifyConnectionInterrupted(PultChannelType type)
{
    if (_pult_channel.type != type)
        return;

    const bool bActive = isProtocolActive();
    stopProtocol();

    if (bActive) {
        OUT_DEBUG_2("PSS_CLOSED\r\n");
        setPultSessionState(PSS_CLOSED); // to reconnect through current channel
    } else {
        OUT_DEBUG_2("PSS_FAILED\r\n");
        setPultSessionState(PSS_FAILED); // to connect through the next channel in the chain
        fa_SetupConnection();
    }
}

void notifyConnectionEstablishingFailed(PultChannelType type)
{
    if (_pult_channel.type != type)
        return;
    closeConnection();
}

void notifyConnectionValidIncoming(PultChannelType type)
{
    pultTxBuffer.setStatus(QueueStatus_InProcessing);
    setPultSessionState(PSS_ESTABLISHED);
    startRecvProtocol();
}
// ============================================================================


static struct PultChannelStash
{
    bool            bStashed;
    PultChannelType type;
    u8              index;
}
_channel_stash;

void stashPultChannel(void)
{
    OUT_DEBUG_2("stashPultChannel()\r\n");
    _channel_stash.bStashed = TRUE;
    _channel_stash.type = getChannel()->type;
    _channel_stash.index = getChannel()->index;
}

void stashPopPultChannel(void)
{
    if (_channel_stash.bStashed)
    {
        _channel_stash.bStashed = FALSE;
        OUT_DEBUG_2("stashPopPultChannel()\r\n");
        updatePultChannel(_channel_stash.type, _channel_stash.index);
    }
}


static bool isLastChannelTypeInChain(void)
{
    return (PCHT_MAX == _pult_channel.type + 1);
}

static void setNextChannelType(void)
{
    bool prefer_gprs = Ar_SIM_currentSettings()->prefer_gprs;

    _pult_channel.attempt = 0;
    _pult_channel.index = 0;

    switch (_pult_channel.type) {
    case PCHT_GPRS_UDP:
        _pult_channel.type = prefer_gprs ? PCHT_VOICECALL : PCHT_SMS;
        break;

    case PCHT_VOICECALL:
        _pult_channel.type = prefer_gprs ? PCHT_SMS : PCHT_GPRS_UDP;
        break;

    case PCHT_SMS:
    default:
        _pult_channel.type = prefer_gprs ? PCHT_GPRS_UDP : PCHT_VOICECALL;
    }
}


s32 preinitPultConnections(void)
{
    OUT_DEBUG_2("preinitPultConnections()\r\n");

    s32 ret = 0;

    // -- setup GPRS
    initializeConnection_UDP(_connections);

    ret = preinitGprs();
    if (ret < RETURN_NO_ERRORS)
        OUT_DEBUG_1("preinitGprs() = %d error.\r\n", ret);

    // -- setup VOICECALL
    initializeConnection_Voicecall(_connections);

    // -- setup SMS
    initializeConnection_SMS(_connections);

    // -- reset channel
    resetPultChannel();

    return ret;
}

const PultChannel *getChannel(void)
{
    return &_pult_channel;
}

void resetChannelAttemptsCounter(void)
{
    OUT_DEBUG_2("resetChannelAttemptsCounter()\r\n");
    _pult_channel.attempt = 0;
}

void setPultSessionState(PultSessionState state)
{
    _pult_channel.state = state;

    switch (state) {
    case PSS_CLOSED:
    case PSS_FAILED:
        Ar_Timer_stop(TMR_PultSession_Timer);
        stashPopPultChannel();

        setPultChannelBusy(FALSE);

        if (PSS_CLOSED == state)
        {
            // FIXME:  if only 2 chenel type
            if (Ar_SIM_currentSettings()->prefer_gprs && _pult_channel.type !=PCHT_GPRS_UDP)
            {
                    updatePultChannel(PCHT_GPRS_UDP, 0);
            }
            else if (!Ar_SIM_currentSettings()->prefer_gprs && _pult_channel.type !=PCHT_VOICECALL)
            {
                    updatePultChannel(PCHT_VOICECALL, 0);
            }

            if (pultTxBuffer.isEmpty()) {
                if (Ar_SIM_currentSettings()->prefer_gprs)
                    fa_activateGprs();
                pultTxBuffer.setStatus(QueueStatus_ReadyToProcessing);
                OUT_DEBUG_2("Go Go Timer 1000\r\n");
                //Ar_Timer_startSingle(TMR_PultSession_Timer, 1000);
            }
            else {
                fa_SetupConnection();   // reestablish connection
            }
        }
        break;

    default:
        break;
    }
}

void setPultChannelBusy(bool busy)
{ _pult_channel.busy = busy; }

void updatePultChannel(PultChannelType type, u8 index)
{
    Ar_Timer_stop(TMR_PultSession_Timer);

    _pult_channel.index = index;
    _pult_channel.type = type;
    _pult_channel.attempt = 0;

    OUT_DEBUG_2("updatePultChannel(type=%s, idx=%d)\r\n", getChannelType_humanize(), index);
}

void resetPultChannel(void)
{
    Ar_Timer_stop(TMR_PultSession_Timer);

    OUT_DEBUG_2("resetPultChannel()\r\n");

    _channel_stash.bStashed = FALSE;    // force reset stash

    _pult_channel.attempt = 0;
    _pult_channel.index = 0;
    _pult_channel.busy = FALSE;
    _pult_channel.state = PSS_CLOSED;

    if (Ar_SIM_canSlotBeUsed(Ar_SIM_currentSlot())) // GPRS can't be activated on invalid SIM card :)
        fa_activateGprs();

    if (Ar_SIM_currentSettings()->prefer_gprs) {
        _pult_channel.type = PCHT_GPRS_UDP;
    } else {
        _pult_channel.type = PCHT_VOICECALL;
    }
}


// ============================================================================
static s32 _setupConnection_Step(void)
{
    const PultChannel *ch = getChannel();

    OUT_DEBUG_2("_setupConnection_Step(): %s @ %s\r\n",
                getChannelType_humanize(), getChannelState_humanize());

    switch (ch->state) {
    case PSS_CLOSED:
        setPultSessionState(PSS_ESTABLISHING);
        break;

    case PSS_ESTABLISHING:
    {
        if (!canCurrentChannelTypeBeUsed()) {
            setPultSessionState(PSS_CHANNEL_TYPE_SWITCHING);
            break;
        }

        if (!canCurrentChannelItemBeUsed()) {
            setPultSessionState(PSS_FAILED);
            break;
        }

// +++++++++++++++++++++++++++++++++++++++++++++++++++++
//  FIXME: temporary patch to block all channels besides UDP and VOICECALL
// should be removed when ALL channels will be defined
        if (PCHT_SMS == ch->type) {
            setNextChannelType();
            break;
        }
// +++++++++++++++++++++++++++++++++++++++++++++++++++++

        s32 ret = establishConnection();
        if (ERR_SHOULD_RETRY_LATER == ret) {
            Ar_Timer_startSingle(TMR_PultSession_Timer, 4000); // retry later
            return RETURN_NO_ERRORS;
        }
        else if (ERR_CHANNEL_ESTABLISHING_WOULDBLOCK == ret) {
            setPultSessionState(PSS_WAIT_FOR_CHANNEL_ESTABLISHED);
            break;
        }
        else if (ERR_SHOULD_STOP_OPERATION == ret) {  // #if defined(NO_SEND_MSG)
            return RETURN_NO_ERRORS;
        }
        else if (ret < RETURN_NO_ERRORS) {
            setPultSessionState(PSS_FAILED);
            break;
        }

        setPultSessionState(PSS_ESTABLISHED);
        startSendProtocol();
        return RETURN_NO_ERRORS;
    }

    case PSS_WAIT_FOR_CHANNEL_ESTABLISHED:
    {
        /* check PSS_WAIT_FOR_CHANNEL_ESTABLISHED timeout */
        if (++ps_waiting_activate_iteration_counter > PSS_WAITING_ITERATION )
        {
            ps_waiting_activate_iteration_counter = 0;
            OUT_DEBUG_2("Reset all channel!\r\n");
            fa_SetupConnection();
            s32 ret = preinitPultConnections();
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("preinitPultConnections() = %d error.\r\n", ret);
            }
            return RETURN_NO_ERRORS;

        }

        OUT_DEBUG_2("ps_waiting_activate_iteration_counter = %d\r\n", ps_waiting_activate_iteration_counter);

        /* if no errors => should stop tracking and just return */
        s32 ret = trackConnectionEstablishing();
        if (ERR_ALREADY_ESTABLISHED == ret) {
            ps_waiting_activate_iteration_counter = 0;
            setPultSessionState(PSS_ESTABLISHED);
            ret = startSendProtocol();
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("startSendProtocol() = %d error\r\n", ret);
                setPultSessionState(PSS_FAILED);
                fa_SetupConnection();
                return ret;
            }
            return RETURN_NO_ERRORS;
        }
        else if (ERR_SHOULD_RETRY_LATER == ret) {
            Ar_Timer_startSingle(TMR_PultSession_Timer, SETUP_CONNECTION_TIMER_INTERVAL);
            return RETURN_NO_ERRORS;
        }
        else if (ret < RETURN_NO_ERRORS) {
            setPultSessionState(PSS_FAILED);
            break;
        }

        return RETURN_NO_ERRORS;
    }

    case PSS_FAILED:
    {
        OUT_DEBUG_8(" *** FAILED for: %s, channel type = %s, channel item = %d, attempt = %d, "
                    "gprs attempts = %d, dtmf attempts = %d, exhausted = %s \r\n",
                    getActiveSimcard_humanize(), getChannelType_humanize(), ch->index,
                    ch->attempt+1, Ar_SIM_currentSettings()->gprs_attempts_qty,
                    Ar_SIM_currentSettings()->voicecall_attempts_qty,
                    (isLastChannelItemForType() && isLastAttemptForChannelItem()) ? "Yes" : "No");

        Ar_Timer_startSingle(TMR_PultSession_Timer, 4000);
        setPultSessionState(PSS_DELAYED_AFTER_FAIL);
        return RETURN_NO_ERRORS;
    }

    case PSS_DELAYED_AFTER_FAIL:
    {
        if (isLastChannelItemForType())
        {
            if (isLastAttemptForChannelItem()) {
                setPultSessionState(PSS_CHANNEL_TYPE_SWITCHING);
            } else {
                _pult_channel.index = 0;
                ++_pult_channel.attempt;
                setPultSessionState(PSS_ESTABLISHING);
            }
        }
        else
        {
            ++_pult_channel.index;
            setPultSessionState(PSS_ESTABLISHING);
        }
        break;
    }

    case PSS_CHANNEL_TYPE_SWITCHING:
    {
        if (isLastChannelTypeInChain())
        {
            /*
             *  If only ONE available SIM card => infinite loop through its channels chain,
             *  else try to switch the SIM slot
             */

            if (SIM_1 == Ar_SIM_currentSlot() && Ar_SIM_canSlotBeUsed(SIM_2)) {
                Ar_SIM_ActivateSlot(SIM_2);
                setPultSessionState(PSS_WAIT_FOR_SIMCARD_SWITCHED);
                break;
            }
            else if (SIM_2 == Ar_SIM_currentSlot() && Ar_SIM_canSlotBeUsed(SIM_1)) {
                Ar_SIM_ActivateSlot(SIM_1);
                setPultSessionState(PSS_WAIT_FOR_SIMCARD_SWITCHED);
                break;
            }
            else {
                OUT_DEBUG_3("CHANNELS' CHAIN IS OVER. JUST WILL START TRAVERSAL AGAIN.\r\n");
                Ar_SIM_ActivateSlot(Ar_SIM_currentSlot());
                setPultSessionState(PSS_WAIT_FOR_SIMCARD_SWITCHED);
                break;
            }
        }

        setNextChannelType();
        setPultSessionState(PSS_ESTABLISHING);
        break;

//        OUT_DEBUG_8(" >>> TRY for: %s, channel type = %s, attempt = %d\r\n",
//                    getActiveSimcard_humanize(), getChannelType_humanize(), ch->attempt+1);
    }

    case PSS_WAIT_FOR_SIMCARD_SWITCHED:
    {
        if (Ar_SIM_isSimCardReady())
        {
            resetPultChannel();
            setPultSessionState(PSS_ESTABLISHING);
            break;
        }
        else if (Ar_SIM_isSlotActivationFailed())
        {
            Ar_SIM_markSimCardFailed(Ar_SIM_newSlot());

            if (SIM_2 == Ar_SIM_newSlot()) {
                if (Ar_SIM_canSlotBeUsed(SIM_1))
                    Ar_SIM_ActivateSlot(SIM_1);
                else if (Ar_SIM_canSlotBeUsed(SIM_2))
                    Ar_SIM_ActivateSlot(SIM_2);
            }
            else if (SIM_1 == Ar_SIM_newSlot()) {
                if (Ar_SIM_canSlotBeUsed(SIM_2))
                    Ar_SIM_ActivateSlot(SIM_2);
                else if (Ar_SIM_canSlotBeUsed(SIM_1))
                    Ar_SIM_ActivateSlot(SIM_1);
            }

            if (!Ar_SIM_canSlotBeUsed(SIM_1) && !Ar_SIM_canSlotBeUsed(SIM_2)) {
                OUT_DEBUG_1("NO AVAILABLE SIM CARDS\r\n");
                Ar_System_setSystemError(SEC_Error4_no_simcard);
                resetPultChannel();
                return ERR_SIM_ACTIVATION_FAILED;
            }

            break;
        }

        Ar_Timer_startSingle(TMR_PultSession_Timer, SETUP_CONNECTION_TIMER_INTERVAL);
        return RETURN_NO_ERRORS;
    }

    default:
        break;
    }

    fa_SetupConnection();
    return RETURN_NO_ERRORS;
}

static s32 _sendMessageToPult(void)
{
    OUT_DEBUG_2("_sendMessageToPult(): %s @ %s\r\n",
                getChannelType_humanize(), getChannelState_humanize());

    s32 ret = getMessageBuilder(NULL)->send();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("PultMessageBuilder::send() = %d error\r\n", ret);
        return ret;
    }
    return RETURN_NO_ERRORS;
}


// -- setup connection
void fa_SetupConnection(void)
{
    Ar_Timer_startSingle(TMR_PultSession_Timer, 1);
}
// -- send the messages
void fa_SendMessageToPult(void)
{
    s32 ret = Ar_Timer_startSingle(TMR_PultSession_Timer, 1);
}

void pultSession_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_PultSession_Timer));

    s32 ret = 0;

    if (PSS_ESTABLISHED == _pult_channel.state)
    {
        ret = _sendMessageToPult();

        Ar_System_setPSSFreezeCounter(0);

        if (ret < RETURN_NO_ERRORS) {
            switch (ret) {
            case ERR_CHANNEL_ESTABLISHING_WOULDBLOCK:
                OUT_DEBUG_1("_sendMessageToPult() = %d error. "
                            "Wait for connection established and then send the message\r\n", ret);
                setPultSessionState(PSS_WAIT_FOR_CHANNEL_ESTABLISHED);
                fa_SetupConnection();
                break;

            default:
                stopProtocol();
                closeConnection();
                if (ERR_BUFFER_IS_EMPTY == ret) {
                    OUT_DEBUG_3("pultTxBuffer is empty. Close connection.\r\n");
                    setPultSessionState(PSS_CLOSED);
                } else {
                    OUT_DEBUG_1("_sendMessageToPult() = %d error\r\n", ret);
                    setPultSessionState(PSS_FAILED);
                    fa_SetupConnection();
                }
            }
        }
    }
    else
    {
        ret = _setupConnection_Step();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("_setupConnection_Step() = %d error.\r\n", ret);
        }
    }
}


// ============================================================================
void pultRxBuffer_timerHandler(void *data)
{
    OUT_DEBUG_3("Timer #%s# is timed out\r\n", Ar_Timer_getTimerNameByCode(TMR_PultRxQueue_Timer));

    if (pultRxBuffer.isEmpty())
        return;

    PultMessageRx m = {0};

    s32 ret = pultRxBuffer.cloneFirst(&m);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("pultRxBuffer::cloneFirst() = %d error\r\n", ret);
        fa_StartPultRxBufferProcessing();
        return;
    }

    if (m.frameNo != m.frameTotal)
        return;

    ret = processPultMessageRx(&m);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("processPultMessageRx() = %d error\r\n", ret);
        if(ERR_BAD_CRC == ret)
            pultRxBuffer.removeAt(0);
    }

    if (RETURN_NO_ERRORS == ret)
        pultRxBuffer.removeAt(0);

    if (!pultRxBuffer.isEmpty())
        fa_StartPultRxBufferProcessing();
    else
        pultRxBuffer.setStatus(QueueStatus_ReadyToProcessing);
}

void fa_StartPultRxBufferProcessing(void)
{ Ar_Timer_startSingle(TMR_PultRxQueue_Timer, 1); }

void ArmingACK_ArmedLEDSwitchON(void)
{
    OUT_DEBUG_7("ArmingACK_ArmedLEDSwitchON() \r\n");

    PultMessage m;
    pultTxBuffer.cloneFirst(&m);
    if (PMQ_RestoralOrSetArmed == m.msg_qualifier &&
            PMC_OpenClose == m.msg_code)
    {

        OUT_DEBUG_1("ArmedLEDSwitch = ON \r\n");
        ArmingGroup *pGroup=getArmingGroupByID(m.group_id);
        if(!pGroup)
            return;
        if(pGroup->p_group_state->state_ident == STATE_ARMED)
        {
        // test_entry
         Ar_System_setPrevETRLed(pGroup,UnitStateOn);

        }
    }
}
