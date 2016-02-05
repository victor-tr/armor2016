#include "connection_udp.h"

#include "core/simcard.h"

#include "pult/gprs_udp.h"

#include "common/debug_macro.h"


static s32 establish(void)
{
    s32 gret = fa_activateGprs();
    if (GPRS_CHANNEL_OK == gret) {
        OUT_DEBUG_3("fa_activateGprs(): OK\r\n");
        return RETURN_NO_ERRORS;
    }
    else if (GPRS_CHANNEL_WOULDBLOCK == gret) {
        OUT_DEBUG_3("fa_activateGprs(): wait for GPRS is UP\r\n");
        return ERR_CHANNEL_ESTABLISHING_WOULDBLOCK;
    }
    else /*if (GPRS_CHANNEL_ERROR == err)*/ {
        OUT_DEBUG_1("fa_activateGprs(): FAIL\r\n");
        deactivateGprs();
        return ERR_GPRS_ACTIVATION_FAIL;
    }
}

static s32 close(void)
{
    stop_pult_protocol_timer();
//    if (!Ar_SIM_currentSettings()->prefer_gprs)
//        deactivateGprs();
    return RETURN_NO_ERRORS;
}

static s32 trackEstablishing(void)
{
    const s32 err = getGprsLastError();
    if (GPRS_CHANNEL_OK == err) {
        return ERR_ALREADY_ESTABLISHED;
    }
    else if (GPRS_CHANNEL_WOULDBLOCK == err) {
        return ERR_SHOULD_RETRY_LATER;
    }
    else /*if (GPRS_CHANNEL_ERROR == err)*/ {
        OUT_DEBUG_1("getGprsLastError() = %d Enum_GPRS_CHANNEL_Error\r\n", err);
        return ERR_GPRS_ACTIVATION_FAIL;
    }
}



static bool canCurrentChannelTypeBeUsed(void)
{
    bool result = TRUE;
    SimSettings *ss = Ar_SIM_currentSettings();
    result = result && ss->udp_ip_list.size >= 1;
    result = result && ss->gprs_attempts_qty > 0;
    return result;
}

static bool isLastAttemptForChannelItem(void)
{
    return getChannel()->attempt + 1 >= Ar_SIM_currentSettings()->gprs_attempts_qty;
}

static bool isLastChannelItemForType(void)
{
    return  getChannel()->index + 1 >= Ar_SIM_currentSettings()->udp_ip_list.size;
}

static bool canCurrentChannelItemBeUsed(void)
{
    bool result = TRUE;
    // begin from '\0' => is empty
    u8 *ip = getIpAddressByIndex(Ar_SIM_currentSlot(), getChannel()->index);
    result = result && ip && ip[0];
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


void initializeConnection_UDP(IConnection *connections[])
{
    connections[PCHT_GPRS_UDP] = &_connection;
}
