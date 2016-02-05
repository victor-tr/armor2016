#include "service/humanize.h"

#include "common/configurator_protocol.h"

#include "codecs/codecs.h"

#include "pult/connection.h"
#include "pult/gprs_udp.h"

#include "core/simcard.h"

#include "ql_stdlib.h"


const char *getChannelType_humanize(void)
{
    u8 code = getChannel()->type;
    switch (code) {
    CASE_RETURN_NAME(PCHT_VOICECALL)
    CASE_RETURN_NAME(PCHT_GPRS_TCP)
    CASE_RETURN_NAME(PCHT_GPRS_UDP)
    CASE_RETURN_NAME(PCHT_SMS)
    CASE_RETURN_NAME(PCHT_CSD)
    DEFAULT_RETURN_CODE(code)
    }
}

const char *getChannelState_humanize(void)
{
    u8 code = getChannel()->state;
    switch (code) {
    CASE_RETURN_NAME(PSS_ESTABLISHED)
    CASE_RETURN_NAME(PSS_ESTABLISHING)
    CASE_RETURN_NAME(PSS_CLOSED)
    CASE_RETURN_NAME(PSS_WAIT_FOR_SIMCARD_SWITCHED)
    CASE_RETURN_NAME(PSS_FAILED)
    CASE_RETURN_NAME(PSS_CHANNEL_TYPE_SWITCHING)
    CASE_RETURN_NAME(PSS_DELAYED_AFTER_FAIL)
    CASE_RETURN_NAME(PSS_WAIT_FOR_CHANNEL_ESTABLISHED)
    DEFAULT_RETURN_CODE(code)
    }
}

const char *getActiveSimcard_humanize(void)
{
    switch (Ar_SIM_currentSlot()) {
    CASE_RETURN_NAME(SIM_1)
    CASE_RETURN_NAME(SIM_2)
    DEFAULT_RETURN_STR("###")
    }
}

const char *getCodec_humanize(void)
{
    u8 code = getMessageBuilder(NULL)->codec->type;
    switch (code) {
    CASE_RETURN_CUSTOM_NAME(PMCT_CODEC_A,   "ARMOR")
    CASE_RETURN_CUSTOM_NAME(PMCT_CODEC_LT9, "LUN_9T")
    CASE_RETURN_CUSTOM_NAME(PMCT_CODEC_LS9, "LUN_9S")
    DEFAULT_RETURN_CODE(code)
    }
}


const char *getUnitTypeByCode(u8 code)
{
    switch (code) {
    CASE_RETURN_CUSTOM_NAME(0, "ALL_TYPES")
    CASE_RETURN_CUSTOM_NAME(OBJ_TIMER, "OBJ_TIMER");
    CASE_RETURN_CUSTOM_NAME(OBJ_ZONE, "Zone");
    CASE_RETURN_CUSTOM_NAME(OBJ_LED, "Led");
    CASE_RETURN_CUSTOM_NAME(OBJ_BELL, "Bell");
    CASE_RETURN_CUSTOM_NAME(OBJ_MASTERBOARD, "Masterboard");
    CASE_RETURN_CUSTOM_NAME(OBJ_M80, "GSM-module");
    CASE_RETURN_CUSTOM_NAME(OBJ_SIM_CARD, "SimCard");
    CASE_RETURN_CUSTOM_NAME(OBJ_GPRS, "OBJ_GPRS");
    CASE_RETURN_CUSTOM_NAME(OBJ_DTMF, "OBJ_DTMF");
    CASE_RETURN_CUSTOM_NAME(OBJ_VPN, "OBJ_VPN");
    CASE_RETURN_CUSTOM_NAME(OBJ_TCPIP_Stack, "OBJ_TCPIP_Stack");
    CASE_RETURN_CUSTOM_NAME(OBJ_CAN_CONTROLLER, "CAN_Controller");
    CASE_RETURN_CUSTOM_NAME(OBJ_AUX_MCU, "AuxMCU");
    CASE_RETURN_CUSTOM_NAME(OBJ_RELAY, "Relay");
    CASE_RETURN_CUSTOM_NAME(OBJ_Output, "OBJ_Output");
    CASE_RETURN_CUSTOM_NAME(OBJ_WatchDog, "OBJ_WatchDog");
    CASE_RETURN_CUSTOM_NAME(OBJ_Source, "OBJ_Source");
    CASE_RETURN_CUSTOM_NAME(OBJ_ETR, "ETR");
    CASE_RETURN_CUSTOM_NAME(OBJ_BUTTON, "Button");
    CASE_RETURN_CUSTOM_NAME(OBJ_TAMPER, "Tamper");
    CASE_RETURN_CUSTOM_NAME(OBJ_BUZZER, "Buzzer");
    CASE_RETURN_CUSTOM_NAME(OBJ_RFID_READER, "RFID_Reader");
    CASE_RETURN_CUSTOM_NAME(OBJ_TOUCHMEMORY_READER, "TouchMemory_Reader");
    CASE_RETURN_CUSTOM_NAME(OBJ_LED_INDICATOR, "LedIndicator");
    CASE_RETURN_CUSTOM_NAME(OBJ_MULTIVIBRATOR, "Multivibrator");
    CASE_RETURN_CUSTOM_NAME(OBJ_RZE, "RZE");
    CASE_RETURN_CUSTOM_NAME(OBJ_RAE, "RAE");
    CASE_RETURN_CUSTOM_NAME(OBJ_SIM_HOLDER, "SIM_Holder");
    CASE_RETURN_CUSTOM_NAME(OBJ_KEYBOARD, "Keyboard");
    DEFAULT_RETURN_CODE(code);
    }
}
