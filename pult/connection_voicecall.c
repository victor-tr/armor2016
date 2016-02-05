#include "connection_voicecall.h"

#include "core/simcard.h"
#include "core/atcommand.h"

#include "pult/voicecall.h"
#include "pult/gprs_udp.h"

#include "common/debug_macro.h"
#include "common/defines.h"


static s32 establish(void)
{
    static u8 nAttempts = 0;

//    deactivateGprs(); FIXME: need other solution
    s32 ret = dialPhoneNumber(getPhoneByIndex(Ar_SIM_currentSlot(), getChannel()->index));
    if (ret != RIL_AT_SUCCESS) {
        OUT_DEBUG_1("dialPhoneNumber() = %d error. Retry later\r\n", ret);
        return (++nAttempts >= VOICECALL_DIALING_ATTEMPTS_THRESHOLD) ?
                    (nAttempts = 0, ERR_DIAL_PHONE_NUMBER_FAIL) :
                    ERR_SHOULD_RETRY_LATER;
    }
    nAttempts = 0;
    start_pult_protocol_timer(VOICECALL_NO_RING_HANGUP_INTERVAL);
    return ERR_CHANNEL_ESTABLISHING_WOULDBLOCK;
}

static s32 close(void)
{
    stop_pult_protocol_timer();
    endVoiceCall();
    return RETURN_NO_ERRORS;
}

static s32 trackEstablishing(void)
{
    s32 ret = Callback_Dial();
    if (ERR_SHOULD_WAIT == ret) {
        return ERR_SHOULD_RETRY_LATER;
    }
    else if (ERR_ALREADY_ESTABLISHED == ret) {
        return ret;
    }
    else if (ret < RETURN_NO_ERRORS)
    {
        OUT_DEBUG_1("Callback_Dial() = %d error\r\n", ret);
        return ret;
    }
    return RETURN_NO_ERRORS;
}



static bool canCurrentChannelTypeBeUsed(void)
{
    bool result = TRUE;
    SimSettings *ss = Ar_SIM_currentSettings();
    result = result && ss->phone_list.size >= 1;
    result = result && ss->voicecall_attempts_qty > 0;
    return result;
}

static bool isLastAttemptForChannelItem(void)
{
    return getChannel()->attempt + 1 >= Ar_SIM_currentSettings()->voicecall_attempts_qty;
}

static bool isLastChannelItemForType(void)
{
    return  getChannel()->index + 1 >= Ar_SIM_currentSettings()->phone_list.size;
}

static bool canCurrentChannelItemBeUsed(void)
{
    bool result = TRUE;
    char *phone = getPhoneByIndex(Ar_SIM_currentSlot(), getChannel()->index);
    result = result && phone && phone[0];
    return result;
}


static IConnection _connection =
{
    &establish,
    &close,
    &trackEstablishing,

    &canCurrentChannelTypeBeUsed,
    &canCurrentChannelItemBeUsed,
    &isLastChannelItemForType,
    &isLastAttemptForChannelItem
};


void initializeConnection_Voicecall(IConnection *connections[])
{
    connections[PCHT_VOICECALL] = &_connection;
}
