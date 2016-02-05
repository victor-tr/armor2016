/*
 * configurator_protocol.h
 *
 * Created: 09.11.2012 12:38:38
 *  Author: user
 */


#ifndef CONFIGURATOR_PROTOCOL_H_
#define CONFIGURATOR_PROTOCOL_H_

#include "defines.h"
#include "../events/events.h"


// =================================================
// from PC configurator
// =================================================

typedef enum _SettingsPacketStructure {
    SPS_MARKER,
    SPS_PKT_KEY,
    SPS_PKT_INDEX,
    SPS_PKT_REPEAT,
    SPS_FILECODE,
    SPS_APPEND_FLAG,
    SPS_BYTES_QTY_H,
    SPS_BYTES_QTY_L,

    SPS_MAX,            // size of a configuration packet without CRC byte
    SPS_PREFIX = SPS_MAX,
    SPS_PREF_N_SUFF = SPS_PREFIX + 1 // +1 for CRC byte
} SettingsPacketStructure;


typedef enum _DbObjectCode {
    // -- Balashov's objects
    OBJ_TIMER   = 1,
      OBJ_Maximum = 2,
      OBJ_Minimum = 3,
    OBJ_ZONE    = 4,
      OBJ_Comparator = 5,
      OBJ_Trigger    = 6,
    OBJ_LED     = 7,
    OBJ_BELL    = 8,
      OBJ_Buffer = 9,
      OBJ_Converter = 10,
    OBJ_MASTERBOARD = 11,
      OBJ_Router = 12,
    OBJ_M80         = 13,
    OBJ_SIM_CARD    = 14,
    OBJ_GPRS        = 15,
    OBJ_DTMF        = 16,
    OBJ_VPN         = 17,
    OBJ_TCPIP_Stack = 18,
      OBJ_TCPIP_Socket = 19,
    OBJ_CAN_CONTROLLER = 20,
    OBJ_AUX_MCU     = 21,
    OBJ_RELAY       = 22,
    OBJ_Output = 23,    // used for GSM-module restart
      OBJ_WatchDog = 24,
    OBJ_Source = 25,
    OBJ_ETR         = 26,   // touchmemory reader device
    OBJ_BUTTON      = 27,
    OBJ_TAMPER      = 28,
    OBJ_BUZZER      = 29,
      OBJ_Switcher = 30,
    OBJ_RFID_READER = 31,
    OBJ_TOUCHMEMORY_READER = 32,
      OBJ_LinkController = 33,
    OBJ_LED_INDICATOR = 34,
    OBJ_MULTIVIBRATOR = 35,
    OBJ_RZE         = 36,   // zones expander
    OBJ_RAE         = 37,   // relays expander
    OBJ_SIM_HOLDER  = 38,

    OBJ_KEYBOARD ,


    // -- additional objects
    OBJ_ARMING_GROUP                = 129,
    OBJ_COMMON_SETTINGS             = 130,
    OBJ_COMMON_AUTOSYNC_SETTINGS    = 131,
    OBJ_KEYBOARD_CODE               = 132,
    OBJ_TOUCHMEMORY_CODE            = 133,

    OBJ_IPLIST_SIM1                 = 134,
    OBJ_IPLIST_SIM2                 = 135,
    OBJ_PHONELIST_SIM1              = 136,
    OBJ_PHONELIST_SIM2              = 137,
    OBJ_PHONELIST_AUX               = 138,

    OBJ_SYSTEM_INFO                 = 139,
    OBJ_SQLITE_DB_FILE              = 140,

    OBJ_EVENT                       = 141,
    OBJ_REACTION                    = 142,
    OBJ_BEHAVIOR_PRESET             = 143,

    OBJ_INTERNAL_TIMER              = 144,
    OBJ_RELATION_ETR_AGROUP         = 145,

    OBJ_FOTA_FILE                   = 146,

    //
    OBJ_MAX,

    // -- command codes
    SEPARATOR_1              = 200,

    PC_OPERATOR_CONFIRMATION_REQUEST,
    PC_OPERATOR_CONFIRM,
    PC_OPERATOR_REJECT,

    PC_CONFIGURATION_BEGIN,
    PC_CONFIGURATION_END,
    PC_CONFIGURATION_DEVICE_ARMED,
    PC_CONFIGURATION_SAVE_AS_WORKING,
    PC_RESTORE_FACTORY_SETTINGS,
    PC_CONFIGURATION_CHECK_TARGET,

    PC_CHECK_DEVICE_DB_VERSION,
    PC_UPLOAD_SETTINGS_START,
    PC_UPLOAD_FACTORY_SETTINGS_START,
    PC_UPLOAD_SETTINGS_NORMAL_END,
    PC_UPLOAD_SETTINGS_FAILED,

    PC_DOWNLOAD_SETTINGS_START,
    PC_DOWNLOAD_FACTORY_SETTINGS_START,
    PC_DOWNLOAD_SETTINGS_NORMAL_END,
    PC_DOWNLOAD_SETTINGS_FAILED,

    PC_DOWNLOAD_SETTINGS_ACK,
    PC_UPLOAD_SETTINGS_ACK,
    PC_DOWNLOAD_SETTINGS_NACK,

    PC_READ_LAST_TOUCHMEMORY_CODE,
    PC_LAST_TOUCHMEMORY_CODE_FAILED,
    PC_LAST_TOUCHMEMORY_CODE_OK,

    PC_UPLOAD_FOTA_FILE_NORMAL_END,
    PC_READ_ADC
} DbObjectCode;


typedef enum _PinTypeCode {
    Pin_NotDefined  = 0,

    Pin_Input       = 1,
    Pin_Output      = 2,
    Pin_Control     = 3,
    Pin_State       = 4,
    Pin_Address     = 5
} PinTypeCode;


// =================================================================
typedef enum _KBD_code_packet {
    KBDCPKT_ID_H,
    KBDCPKT_ID_L,
    KBDCPKT_KEY_H,
    KBDCPKT_KEY_L = KBDCPKT_KEY_H + KEYBOARD_MAX_KEY_SIZE - 1,
    KBDCPKT_KEY_SIZE,
    KBDCPKT_ALIAS_H,
    KBDCPKT_ALIAS_L = KBDCPKT_ALIAS_H + ALIAS_LEN - 1,
    KBDCPKT_ACTION,
    KBDCPKT_ARMING_GROUP_ID_H,
    KBDCPKT_ARMING_GROUP_ID_L,

    KBDCPKT_MAX
} KBD_code_packet;


// =================================================================
typedef enum _Touchmemory_code_packet {
    TMCPKT_ID_H,
    TMCPKT_ID_L,
    TMCPKT_KEY_H,
    TMCPKT_KEY_L = TMCPKT_KEY_H + TOUCHMEMORY_MAX_KEY_SIZE - 1,
    TMCPKT_KEY_SIZE,
    TMCPKT_ALIAS_H,
    TMCPKT_ALIAS_L = TMCPKT_ALIAS_H + ALIAS_LEN - 1,
    TMCPKT_ACTION,
    TMCPKT_ARMING_GROUP_ID_H,
    TMCPKT_ARMING_GROUP_ID_L,

    TMCPKT_MAX
} Touchmemory_code_packet;


// =================================================================
typedef enum _Touchmemory_code_value_packet {
    TMVPKT_ETR_UIN_H,
    TMVPKT_ETR_UIN_L,
    TMVPKT_KEY_SIZE,
    TMVPKT_KEY_H,
    TMVPKT_KEY_L = TMVPKT_KEY_H + TOUCHMEMORY_MAX_KEY_SIZE - 1,

    TMVPKT_MAX
} Touchmemory_code_value_packet;


// =================================================================
typedef enum _Arming_group_packet {
    AGPKT_ID_H,
    AGPKT_ID_L,
    AGPKT_ALIAS_H,
    AGPKT_ALIAS_L = AGPKT_ALIAS_H + ALIAS_LEN - 1,

    AGPKT_RELATED_ETRs_QTY_H,
    AGPKT_RELATED_ETRs_QTY_L,

    AGPKT_MAX
} Arming_group_packet;


// =================================================================
typedef enum _Common_settings_packet {
    CSPKT_CODEC_TYPE,
    CSPKT_ENTRY_DELAY_H,
    CSPKT_ENTRY_DELAY_L,
    CSPKT_ARMING_DELAY_H,
    CSPKT_ARMING_DELAY_L,
    CSPKT_DEBUG_LEVEL,
    CSPKT_AUX_PHONELIST_DEFAULT_SIZE,

    CSPKT_DTMF_PULSE_MS_H,
    CSPKT_DTMF_PULSE_MS_L,
    CSPKT_DTMF_PAUSE_MS_H,
    CSPKT_DTMF_PAUSE_MS_L,

    CSPKT_ADC_A_MANTISSA_H,
    CSPKT_ADC_A_MANTISSA_L,
    CSPKT_ADC_A_DECIMAL_H,
    CSPKT_ADC_A_DECIMAL_L,
    CSPKT_ADC_B_MANTISSA_H,
    CSPKT_ADC_B_MANTISSA_L,
    CSPKT_ADC_B_DECIMAL_H,
    CSPKT_ADC_B_DECIMAL_L,

    CSPKT_MAX,
    CSPKT_ADC_Precision = 10000
} Common_settings_packet;

typedef enum _PultMessageCodecType {
    PMCT_CODEC_A,
    PMCT_CODEC_LT9,
    PMCT_CODEC_LS9
} PultMessageCodecType;


// =================================================================
typedef enum _Zone_packet {
    ZPKT_ID_H,
    ZPKT_ID_L,
    ZPKT_HUMANIZED_ID_H,
    ZPKT_HUMANIZED_ID_L,
    ZPKT_ALIAS_H,
    ZPKT_ALIAS_L = ZPKT_ALIAS_H + ALIAS_LEN - 1,
    ZPKT_SUIN,
    ZPKT_ENABLE_FLAG,
    ZPKT_ZONE_TYPE,
    ZPKT_ARMING_GROUP_ID_H,
    ZPKT_ARMING_GROUP_ID_L,
    ZPKT_PARENTDEV_UIN_H,
    ZPKT_PARENTDEV_UIN_L,
    ZPKT_PARENTDEV_TYPE,
    ZPKT_LOOP_DAMAGE_NOTIFICATION_FLAG,

    ZPKT_MAX
} Zone_packet;


// =================================================================
typedef enum _Relay_packet {
    RPKT_ID_H,
    RPKT_ID_L,
    RPKT_HUMANIZED_ID_H,
    RPKT_HUMANIZED_ID_L,
    RPKT_ALIAS_H,
    RPKT_ALIAS_L = RPKT_ALIAS_H + ALIAS_LEN - 1,
    RPKT_SUIN,
    RPKT_ENABLE_FLAG,
    RPKT_REMOTE_CONTROL_FLAG,
    RPKT_NOTIFY_ON_STATE_CHANGED_FLAG,
    RPKT_PARENTDEV_UIN_H,
    RPKT_PARENTDEV_UIN_L,
    RPKT_PARENTDEV_TYPE,

    RPKT_LOAD_TYPE,
    RPKT_GROUP_ID_H,
    RPKT_GROUP_ID_L,

    RPKT_MAX
} Relay_packet;


// =================================================================
typedef enum _Led_packet {
    LEDPKT_ID_H,
    LEDPKT_ID_L,
    LEDPKT_HUMANIZED_ID_H,
    LEDPKT_HUMANIZED_ID_L,
    LEDPKT_ALIAS_H,
    LEDPKT_ALIAS_L = LEDPKT_ALIAS_H + ALIAS_LEN - 1,
    LEDPKT_SUIN,
    LEDPKT_ENABLE_FLAG,

    LEDPKT_PARENTDEV_UIN_H,
    LEDPKT_PARENTDEV_UIN_L,
    LEDPKT_PARENTDEV_TYPE,

    LEDPKT_ARMING_LED_FLAG,
    LEDPKT_GROUP_ID_H,
    LEDPKT_GROUP_ID_L,

    LEDPKT_MAX
} Led_packet;


// =================================================================
typedef enum _Button_packet {
    BTNPKT_ID_H,
    BTNPKT_ID_L,
    BTNPKT_HUMANIZED_ID_H,
    BTNPKT_HUMANIZED_ID_L,
    BTNPKT_ALIAS_H,
    BTNPKT_ALIAS_L = BTNPKT_ALIAS_H + ALIAS_LEN - 1,
    BTNPKT_SUIN,
    BTNPKT_ENABLE_FLAG,

    BTNPKT_PARENTDEV_UIN_H,
    BTNPKT_PARENTDEV_UIN_L,
    BTNPKT_PARENTDEV_TYPE,

    BTNPKT_MAX
} Button_packet;


// =================================================================
typedef enum _Bell_packet {
    BELLPKT_ID_H,
    BELLPKT_ID_L,
    BELLPKT_HUMANIZED_ID_H,
    BELLPKT_HUMANIZED_ID_L,
    BELLPKT_ALIAS_H,
    BELLPKT_ALIAS_L = BELLPKT_ALIAS_H + ALIAS_LEN - 1,
    BELLPKT_SUIN,
    BELLPKT_ENABLE_FLAG,
    BELLPKT_REMOTE_CONTROL_FLAG,
    BELLPKT_PARENTDEV_UIN_H,
    BELLPKT_PARENTDEV_UIN_L,
    BELLPKT_PARENTDEV_TYPE,

    BELLPKT_MAX
} Bell_packet;


// =================================================================
//          device structure block
// =================================================================
#define DEV_STRUCTURE(begin_after) (begin_after + 1)

typedef enum {
    DSBLK_ZONE_QTY,
    DSBLK_RELAY_QTY,
    DSBLK_LEDS_QTY,
    DSBLK_BUTTON_QTY,
    DSBLK_TAMPER_QTY,
    DSBLK_BUZZER_QTY,
    DSBLK_RFID_READER_QTY,
    DSBLK_TOUCHMEMORY_READER_QTY,

    DSBLK_MAX
} DeviceStructureBlock;


// =================================================================
typedef enum _Expander_packet {
    EPKT_PARENT_ID_H,
    EPKT_PARENT_ID_L,
    EPKT_ID_H,
    EPKT_ID_L,
    EPKT_UIN_H,
    EPKT_UIN_L,
    EPKT_ALIAS_H,
    EPKT_ALIAS_L = EPKT_ALIAS_H + ALIAS_LEN - 1,
    EPKT_TYPE,

//    EPKT_DEV_STRUCTURE_H,
//    EPKT_DEV_STRUCTURE_L = EPKT_DEV_STRUCTURE_H + DSBLK_MAX - 1,

    EPKT_MAX
} Expander_packet;


// =================================================================
typedef enum _Masterboard_packet {
    MBPKT_PARENT_ID_H,
    MBPKT_PARENT_ID_L,
    MBPKT_ID_H,
    MBPKT_ID_L,
    MBPKT_UIN_H,
    MBPKT_UIN_L,
    MBPKT_ALIAS_H,
    MBPKT_ALIAS_L = MBPKT_ALIAS_H + ALIAS_LEN - 1,

//    MBPKT_DEV_STRUCTURE_H,
//    MBPKT_DEV_STRUCTURE_L = MBPKT_DEV_STRUCTURE_H + DSBLK_MAX - 1,

    MBPKT_MAX
} Masterboard_packet;


// =================================================================
typedef enum _ETR_packet {
    ETRPKT_PARENT_ID_H,
    ETRPKT_PARENT_ID_L,
    ETRPKT_ID_H,
    ETRPKT_ID_L,
    ETRPKT_UIN_H,
    ETRPKT_UIN_L,
    ETRPKT_ALIAS_H,
    ETRPKT_ALIAS_L = ETRPKT_ALIAS_H + ALIAS_LEN - 1,
    ETRPKT_ETR_TYPE,

//    ETRPKT_DEV_STRUCTURE_H,
//    ETRPKT_DEV_STRUCTURE_L = ETRPKT_DEV_STRUCTURE_H + DSBLK_MAX - 1,

    ETRPKT_RELATED_GROUPS_QTY_H,
    ETRPKT_RELATED_GROUPS_QTY_L,

    ETRPKT_MAX
} ETR_packet;


// =================================================================
typedef enum _PhoneNumber_packet {
    PHNPKT_ID,
    PHNPKT_PHONENUMBER_H,
    PHNPKT_PHONENUMBER_L  = PHNPKT_PHONENUMBER_H + PHONE_LEN - 1,
    PHNPKT_ALLOWED_SIMCARD,

    PHNPKT_MAX
} PhoneNumber_packet;


// =================================================================
typedef enum _IPaddress_packet {
    IPAPKT_ID,
    IPAPKT_IPADDRESS_0,
    IPAPKT_IPADDRESS_1,
    IPAPKT_IPADDRESS_2,
    IPAPKT_IPADDRESS_3,

    IPAPKT_MAX
} IPaddress_packet;

// =================================================================
typedef enum _SimCard_packet {
    SIMPKT_ID,

    SIMPKT_USABLE,
    SIMPKT_PREFER_GPRS,
    SIMPKT_ALLOW_SMS,

    SIMPKT_GPRS_ATTEMPTS_QTY,
    SIMPKT_VOICECALL_ATTEMPTS_QTY,
    SIMPKT_UDP_IPLIST_DEFAULT_SIZE,
    SIMPKT_TCP_IPLIST_DEFAULT_SIZE,
    SIMPKT_PHONELIST_DEFAULT_SIZE,

    SIMPKT_UDP_DESTPORT_H,
    SIMPKT_UDP_DESTPORT_L,
    SIMPKT_UDP_LOCALPORT_H,
    SIMPKT_UDP_LOCALPORT_L,

    SIMPKT_TCP_DESTPORT_H,
    SIMPKT_TCP_DESTPORT_L,
    SIMPKT_TCP_LOCALPORT_H,
    SIMPKT_TCP_LOCALPORT_L,

    SIMPKT_APN_H,
    SIMPKT_APN_L = SIMPKT_APN_H + APN_LEN - 1,
    SIMPKT_LOGIN_H,
    SIMPKT_LOGIN_L = SIMPKT_LOGIN_H + CREDENTIALS_LEN - 1,
    SIMPKT_PASS_H,
    SIMPKT_PASS_L = SIMPKT_PASS_H + CREDENTIALS_LEN - 1,

    SIMPKT_MAX
} SimCard_packet;


// =================================================================
typedef enum _SystemInfo_packet {
    SINFPKT_SETTINGS_SIGNATURE_0,
    SINFPKT_SETTINGS_SIGNATURE_1,
    SINFPKT_SETTINGS_SIGNATURE_2,
    SINFPKT_SETTINGS_SIGNATURE_3,
    SINFPKT_SETTINGS_IDENT_STR_H,
    SINFPKT_SETTINGS_IDENT_STR_L = SINFPKT_SETTINGS_IDENT_STR_H + SYSTEM_INFO_STRINGS_LEN - 1,

    SINFPKT_MAX
} SystemInfo_packet;


// =================================================================
typedef enum _BehaviorPreset_packet {
    BHPPKT_ID_H,
    BHPPKT_ID_L,
    BHPPKT_ALIAS_H,
    BHPPKT_ALIAS_L = BHPPKT_ALIAS_H + ALIAS_LEN - 1,
    BHPPKT_PULSES_IN_BATCH,
    BHPPKT_PULSE_LEN,
    BHPPKT_PAUSE_LEN,
    BHPPKT_BATCH_PAUSE_LEN,
    BHPPKT_BEHAVIOR_TYPE,

    BHPPKT_MAX
} BehaviorPreset_packet;


// =================================================================
typedef enum _Event_packet {
    EVTPKT_ID_H,
    EVTPKT_ID_L,
    EVTPKT_ALIAS_H,
    EVTPKT_ALIAS_L = EVTPKT_ALIAS_H + ALIAS_LEN - 1,
    EVTPKT_EVENT,
    EVTPKT_EMITTER_TYPE,
    EVTPKT_EMITTER_ID_H,
    EVTPKT_EMITTER_ID_L,
    EVTPKT_SUBSCRIBERS_QTY,

    EVTPKT_MAX
} Event_packet;


// =================================================================

typedef enum {
    PB_SwitchOff,
    PB_SwitchOn,
    PB_UsePreset
} PerformerBehavior;

typedef enum _Reaction_packet {
    RCNPKT_ID_H,
    RCNPKT_ID_L,
    RCNPKT_ALIAS_H,
    RCNPKT_ALIAS_L = RCNPKT_ALIAS_H + ALIAS_LEN - 1,
    RCNPKT_EVENT_ID_H,
    RCNPKT_EVENT_ID_L,
    RCNPKT_PERFORMER_TYPE,
    RCNPKT_PERFORMER_ID_H,
    RCNPKT_PERFORMER_ID_L,
    RCNPKT_PERFORMER_BEHAVIOR,
    RCNPKT_BEHAVIOR_PRESET_ID_H,
    RCNPKT_BEHAVIOR_PRESET_ID_L,
    RCNPKT_VALID_STATES_H,
    RCNPKT_VALID_STATES_L = RCNPKT_VALID_STATES_H + STATE_MAX - 1,
    RCNPKT_REVERSIBLE_FLAG,
    RCNPKT_REVERSE_PERFORMER_BEHAVIOR,
    RCNPKT_REVERSE_BEHAVIOR_PRESET_ID_H,
    RCNPKT_REVERSE_BEHAVIOR_PRESET_ID_L,

    RCNPKT_MAX
} Reaction_packet;


// =================================================================
typedef enum _InternalTimer_packet {
    ITMPKT_ID_H,
    ITMPKT_ID_L,
    ITMPKT_ALIAS_H,
    ITMPKT_ALIAS_L = ITMPKT_ALIAS_H + ALIAS_LEN - 1,
    ITMPKT_OWNER_TYPE,
    ITMPKT_OWNER_UIN_H,
    ITMPKT_OWNER_UIN_L,
    ITMPKT_INTERVAL_H,
    ITMPKT_INTERVAL_L,

    ITMPKT_MAX
} InternalTimer_packet;


// =================================================================
typedef enum {
    REAGPKT_ETR_ID_H,
    REAGPKT_ETR_ID_L,
    REAGPKT_GROUP_ID_H,
    REAGPKT_GROUP_ID_L,

    REAGPKT_MAX
} RelationETRArmingGroup_packet;


// =================================================================
typedef enum _ADC_value_packet {
    ADCPKT_VALUE_H,
    ADCPKT_VALUE_L,

    ADCPKT_MAX
} ADC_value_packet;


#endif /* CONFIGURATOR_PROTOCOL_H_ */
