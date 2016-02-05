#include "connection_sms.h"
#include "core/simcard.h"


static s32 establish(void)
{
    return RETURN_NO_ERRORS;
}

static s32 trackEstablishing(void)
{
    return RETURN_NO_ERRORS;
}

static s32 close(void)
{
    return 0;
}



static bool canCurrentChannelTypeBeUsed(void)
{
    bool result = TRUE;
    SimSettings *ss = Ar_SIM_currentSettings();
    result = result && ss->allow_sms_to_pult;
    result = result && ss->phone_list.size >= 1;
    return result;
}

static bool isLastAttemptForChannelItem(void)
{
    // for SMS channels the condition "exhausted == TRUE" is valid by default
    return TRUE;
}

static bool isLastChannelItemForType(void)
{
    return  getChannel()->index + 1 >= Ar_SIM_currentSettings()->phone_list.size;
}

static bool canCurrentChannelItemBeUsed(void)
{
    return TRUE;
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


void initializeConnection_SMS(IConnection *connections[])
{
    connections[PCHT_SMS] = &_connection;
}
