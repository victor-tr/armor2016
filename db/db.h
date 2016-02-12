/*
 * File:    db.h
 *
 * Created: 09.07.2012 11:30:33
 * Author:  remicollab@gmail.com
 */
#ifndef DB_H_
#define DB_H_

#include "ql_type.h"
#include "ql_error.h"

#include "common/errors.h"
#include "common/configurator_protocol.h"

#include "codecs/message_codes.h"
#include "codecs/codecs.h"

#include "pult/connection.h"


typedef struct {
    u16 item_qty;
    u16 item_size;
    DbObjectCode item_type;
    void *d;
} AbstractTable;


typedef enum {
    UnitStateOff = 0,
    UnitStateOn  = 1,
    UnitStateMultivibrator = 2
} PerformerUnitState;


typedef struct _GroupState GroupState;
typedef struct _Zone Zone;
typedef struct _ParentDevice ParentDevice;



/************************************************************************
 * REACTIONS
 ************************************************************************/
typedef struct _Reaction {
    u16          id;
    u8           alias[ALIAS_LEN + 1];
    u16          event_id;
    DbObjectCode performer_type;
    u16          performer_id;
    u8           performer_behavior;
    u16          behavior_preset_id;
    bool         valid_states[STATE_MAX];
    bool         bReversible;
    u8           reverse_behavior;
    u16          reverse_preset_id;
} Reaction;

typedef struct _Reaction_tbl {
    u16       size;
    Reaction *t;
} Reaction_tbl;

//
s32 createRuntimeTable_Reactions(void);
s32 destroyRuntimeTable_Reactions(void);
Reaction *getReactionByID(u16 id);
Reaction_tbl *getReactionTable(s32 *error);

bool isReversibleReaction(Reaction *pReaction);


/************************************************************************
 * TRIGGER EVENTS
 ************************************************************************/
typedef struct _ReactionPointers_tbl {
    u8         size;
    Reaction **t;
} ReactionPointers_tbl;

typedef struct _Event {
    u16          id;
    u8           alias[ALIAS_LEN + 1];
    u8           event_code;
    DbObjectCode emitter_type;
    u16          emitter_id;
    u8           subscribers_qty;
    ReactionPointers_tbl tbl_subscribers;
} Event;

typedef struct {
    u16    size;
    Event *t;
} Event_tbl;

//
s32 createRuntimeTable_Events(void);
s32 destroyRuntimeTable_Events(void);
Event *getEventByID(u16 id);
Event_tbl *getEventTable(s32 *error);

s32 fillEventSubscribersTable(Event *pEvent);
s32 freeEventSubscribersTable(Event *pEvent);

s32 processReactions(Event *pTrigger, GroupState *pState);


/************************************************************************
 * BEHAVIOR PRESETS
 ************************************************************************/
typedef struct _BehaviorPreset {
    u16 id;
    u8  alias[ALIAS_LEN + 1];
    u8  pulses_in_batch;
    u8  pulse_len;
    u8  pause_len;
    u8  batch_pause_len;
    u8  behavior_type;

} BehaviorPreset;

typedef struct {
    u16             size;
    BehaviorPreset *t;
} BehaviorPreset_tbl;

//
s32 createRuntimeTable_BehaviorPresets(void);
s32 destroyRuntimeTable_BehaviorPresets(void);
BehaviorPreset *getBehaviorPresetByID(u16 id);
BehaviorPreset_tbl *getBehaviorPresetTable(s32 *error);

bool isBehaviorPresetEqual(BehaviorPreset *first, BehaviorPreset *second);


//************************************************************************
//* iBUTTON CODES
//************************************************************************/
typedef struct _TouchmemoryCode {
    u16 id;
    u8  alias[ALIAS_LEN + 1];
    u8  value[TOUCHMEMORY_MAX_KEY_SIZE];
    u8  len;
    u8  action;
    u16 group_id;

    Event *events[1];
} TouchmemoryCode;

//
typedef struct _TouchmemoryCode_tbl {
    u16              size;
    TouchmemoryCode *t;
} TouchmemoryCode_tbl;

s32 createRuntimeTable_TouchmemoryCodes(void);
s32 destroyRuntimeTable_TouchmemoryCodes(void);
TouchmemoryCode_tbl *getTouchmemoryCodeTable(s32 *error);
TouchmemoryCode *getTouchmemoryCodeByValue(u8 *value);
TouchmemoryCode *getTouchmemoryCodeByID(u16 id);


//************************************************************************
//* KEYBOARD CODES
//************************************************************************/
typedef struct _KeyboardCode {
    u16 id;
    u8  alias[ALIAS_LEN + 1];
    u8  value[KEYBOARD_MAX_KEY_SIZE];
    u8  len;
    u8  action;
    u16 group_id;

    Event *events[1];
} KeyboardCode;

//
typedef struct _KeyboardCode_tbl {
    u16           size;
    KeyboardCode *t;
} KeyboardCode_tbl;

s32 createRuntimeTable_KeyboardCodes(void);
s32 destroyRuntimeTable_KeyboardCodes(void);
KeyboardCode_tbl *getKeyboardCodeTable(s32 *error);
KeyboardCode *getKeyboardCodeByValue(u8 *value);
KeyboardCode *getKeyboardCodeByID(u16 id);


//************************************************************************
//* RELATIONS: ARMING GROUPS - ETRs
//************************************************************************/
typedef struct {
    u16 ETR_id;
    u16 group_id;
} RelationETRArmingGroup;

typedef struct {
    u16 size;
    RelationETRArmingGroup *t;
} RelationETRArmingGroup_tbl;

s32 createRuntimeTable_RelationsETRArmingGroup(void);
s32 destroyRuntimeTable_RelationsETRArmingGroup(void);
RelationETRArmingGroup_tbl *getRelationsETRArmingGroupTable(s32 *error);


//************************************************************************
//* ARMING GROUPS
//************************************************************************/
typedef struct {
    u16   size;
    ParentDevice **t;
} ParentDevsPointers_tbl;

typedef struct _ArmingGroup {
    u16 id;
    u8  alias[ALIAS_LEN + 1];

    GroupState    *p_group_state;
    GroupState    *p_prev_group_state;
    AbstractTable *delayed_zones;
    u16            arming_zoneDelay_counter;
    u16            disarming_zoneDelay_counter;
    Zone          *entry_delay_zone;
    Event         *events[1];

    u16                    related_ETRs_qty;
    ParentDevsPointers_tbl tbl_related_ETRs;
    ParentDevice  *change_status_ETR;
} ArmingGroup;

//
typedef struct _ArmingGroup_tbl {
    u16          size;
    ArmingGroup *t;
} ArmingGroup_tbl;

//
s32 createRuntimeTable_ArmingGroups(void);
s32 destroyRuntimeTable_ArmingGroups(void);
ArmingGroup *getArmingGroupByID(u16 group_id);
ArmingGroup_tbl *getArmingGroupTable(s32 *error);

bool isGroupAlarmed(ArmingGroup *pGroup);

s32 fillRelatedETRsTable(ArmingGroup *pGroup);
s32 freeRelatedETRsTable(ArmingGroup *pGroup);


/************************************************************************
 * PARENT DEVICES
 ************************************************************************/
typedef struct {
    u8 zone_qty;
    u8 relay_qty;
    u8 led_qty;
    u8 button_qty;
    u8 tamper_qty;
    u8 buzzer_qty;
    u8 rfid_reader_qty;
    u8 touchmemory_reader_qty;
} InternalDevStructure;

// -- MasterBoard
typedef struct _MasterBoard {
    u8  alias[ALIAS_LEN + 1];
    InternalDevStructure structure;
    bool bMcuApproved;
    bool bMcuConnectionRequested;
} MasterBoard;

// -- ETR
typedef struct {
    u8 current_led_decade;
    u8 led_decade_state_masks_low[10][10];
    u8 led_decade_state_mask_high[10];
} EtrLedIndicator;

typedef struct _EtrBuzzer {
    PerformerUnitState state;
    BehaviorPreset     behavior_preset;
    u16                puin;
    u16                counter; // counter of armind-disarming activ process
} EtrBuzzer;

typedef struct {
    u16           size;
    ArmingGroup **t;
} ArmingGroupPointers_tbl;

typedef struct _ETR {
    u8  alias[ALIAS_LEN + 1];
    u8 type;
    InternalDevStructure structure;
    u8  last_received_keycode[TOUCHMEMORY_MAX_KEY_SIZE];
    bool bBlocked;  // set to TRUE when waiting for key to send to configurator SW

    EtrLedIndicator led_indicator;
    EtrBuzzer       buzzer;

    u16 related_groups_qty;
    ArmingGroupPointers_tbl tbl_related_groups;
} ETR;

// -- Zone Expander
typedef struct _RZE {
    u8  alias[ALIAS_LEN + 1];
    InternalDevStructure structure;
} RZE;

// -- Relay Expander
typedef struct _RAE {
    u8  alias[ALIAS_LEN + 1];
    InternalDevStructure structure;
} RAE;

typedef union {
    MasterBoard masterboard;
    ETR         etr;
    RZE         zone_expander;
    RAE         relay_expander;
} ParentDevData;


typedef enum {
    UnitDisconnectedReported,
    UnitConnected,
    UnitConnectedReported,
    UnitAfterPowerOn
} ParentDeviceConnectionStateValue;

typedef struct {
    u16 counter;
    ParentDeviceConnectionStateValue value;
} ParentDeviceConnectionState;


// -- ParentDev object
typedef struct _ParentDevice {
    u16 parent_id;
    DbObjectCode  type;
    u16           uin;
    u16           id;
    ParentDeviceConnectionState connection_state;
    ParentDevData d;
} ParentDevice;

typedef struct {
    u16 size;
    ParentDevice *t;
} ParentDevice_tbl;

s32 createRuntimeTable_ParentDevices(void);
s32 destroyRuntimeTable_ParentDevices(void);
ParentDevice *getParentDeviceByUIN(u16 uin, DbObjectCode type);
ParentDevice *getParentDeviceByID(u16 id, DbObjectCode type);
ParentDevice_tbl *getParentDevicesTable(s32 *error);

bool isParentDeviceConnected(u16 uin, DbObjectCode type);

// ETR related functions
s32 requestLastReceivedTouchMemoryCode(u16 etrUIN);
s32 sendLastReceivedTouchMemoryCode(u16 etrUIN, const u8 *last_key);
void clearETRsBlocking(void);
void finishETRWatingKey(ParentDevice *pParentETR);

s32 fillRelatedGroupsTable(ParentDevice *pParent);
s32 freeRelatedGroupsTable(ParentDevice *pParent);

void initETR(ParentDevice *pEtr);
void setETRLedIndicatorLedState(ParentDevice *pEtr, u8 zone_id, PerformerUnitState state);
void setETRLedIndicatorDecade(ParentDevice *pEtr, u8 new_decade_id);
bool isETRRelatedToGroup(const u16 group_id, const ETR *pEtr);
void batchSetETRLedIndicatorsLedState(Zone *pZone, PerformerUnitState state);

s32 setEtrBuzzerUnitState(EtrBuzzer *pBuzzer, PerformerUnitState state, BehaviorPreset *pBehavior);
s32 setEtrBuzzerUnitStateALL(EtrBuzzer *pBuzzer, PerformerUnitState state, BehaviorPreset *pBehavior);


/************************************************************************
 * ZONES
 ************************************************************************/
//
typedef enum _ZoneBusType {
    ZBT_SerialLine,
    ZBT_ParallelLine
} ZoneBusType;

typedef enum _ZoneType {
    ZONE_GENERAL,
    ZONE_FIRE,
    ZONE_ENTRY_DELAY,
    ZONE_WALK_THROUGH,
    ZONE_24_HOUR,
    ZONE_ATHOME_ARMING,
    ZONE_PANIC,
    ZONE_TRACK_EVENT,

    ZONE_MAX
} ZoneType;

typedef struct _Zone {
    u16             id;
    u16             humanized_id;
    u8              alias[ALIAS_LEN + 1];
    u8              suin;
    bool            enabled;
    ZoneType        zone_type;
    u16             group_id;
    u16             puin;
    DbObjectCode    ptype;
    bool            damage_notification_enabled;

    ParentDevice   *parent_device;
    u8              zoneDamage_counter;
    Event          *events[5];  // 5 zone event types now exists
    u8              last_event_type;
} Zone;

//
typedef struct _Zone_tbl {
    u16   size;
    Zone *t;
} Zone_tbl;

//
s32 createRuntimeTable_Zones(void);
s32 destroyRuntimeTable_Zones(void);
Zone *getZoneByID(u16 zone_id);
Zone_tbl *getZoneTable(s32 *error);

bool isArmingDelayActive(Zone *pZone);
bool isDisarmingDelayActive(Zone *pZone);
void startArmingDelay(ArmingGroup *group);
void startDisarmingDelay(ArmingGroup *group, Zone *entry_delay_zone);
void stopBothDelays(ArmingGroup *group);

Zone *getZoneByParent(u16 puin, DbObjectCode ptype, u16 suin);


/************************************************************************
 * RELAYS
 ************************************************************************/
typedef struct _Relay {
    u16             id;
    u16             humanized_id;
    u8              alias[ALIAS_LEN + 1];
    u8              suin;
    bool            enabled;
    bool            remote_control;
    bool            notify_on_state_changed;
    u16             puin;
    DbObjectCode    ptype;

    ParentDevice   *parent_device;

    // state
    PerformerUnitState state;
    BehaviorPreset     behavior_preset;

    u8              load_type;
    u16             group_id;
} Relay;

//
typedef struct _Relay_tbl {
    u16    size;
    Relay *t;
} Relay_tbl;

//
s32 createRuntimeTable_Relays(void);
s32 destroyRuntimeTable_Relays(void);
Relay *getRelayByID(u16 relay_id);
Relay_tbl *getRelayTable(s32 *error);

Relay *getRelayByParent(u16 puin, DbObjectCode ptype, u16 suin);

// <pBehavior> can be NULL if <state> is not <Multivibrator>
s32 setRelayUnitState(Relay *pRelay, PerformerUnitState state, BehaviorPreset *pBehavior);
s32 setRelayUnitStateAll(Relay *pRelay, PerformerUnitState state, BehaviorPreset *pBehavior);


/************************************************************************
 * LEDS
 ************************************************************************/
typedef struct _Led {
    u16             id;
    u16             humanized_id;
    u8              alias[ALIAS_LEN + 1];
    u8              suin;
    bool            enabled;
    u16             puin;
    DbObjectCode    ptype;

    ParentDevice   *parent_device;

    // state
    PerformerUnitState state;
    BehaviorPreset     behavior_preset;

    bool            b_arming_led;
    u16             group_id;
} Led;

//
typedef struct _Led_tbl {
    u16  size;
    Led *t;
} Led_tbl;

//
s32 createRuntimeTable_Leds(void);
s32 destroyRuntimeTable_Leds(void);
Led *getLedByID(u16 led_id);
Led_tbl *getLedTable(s32 *error);

Led *getLedByParent(u16 puin, DbObjectCode ptype, u16 suin);
s32 setLedUnitState(Led *pLed, PerformerUnitState state, BehaviorPreset *pBehavior);
s32 setLedUnitStateALL(Led *pLed, PerformerUnitState state, BehaviorPreset *pBehavior);


/************************************************************************
 * AUDIO
 ************************************************************************/
s32 setAudioUnitState(u16 puin , PerformerUnitState state);

/************************************************************************
 * BELLS
 ************************************************************************/
typedef struct _Bell {
    u16             id;
    u16             humanized_id;
    u8              alias[ALIAS_LEN + 1];
    u8              suin;
    bool            enabled;
    bool            remote_control;
    u16             puin;
    DbObjectCode    ptype;

    ParentDevice   *parent_device;

    // state
    PerformerUnitState state;
    BehaviorPreset     behavior_preset;
} Bell;

//
typedef struct _Bell_tbl {
    u16   size;
    Bell *t;
} Bell_tbl;

//
s32 createRuntimeTable_Bells(void);
s32 destroyRuntimeTable_Bells(void);
Bell *getBellByID(u16 bell_id);
Bell_tbl *getBellTable(s32 *error);

Bell *getBellByParent(u16 puin, DbObjectCode ptype, u16 suin);
s32 setBellUnitState(Bell *pBell, PerformerUnitState state, BehaviorPreset *pBehavior);


/************************************************************************
 * BUZZER
 ************************************************************************/
//typedef struct _Buzzer {
//    u16             id;
//    u16             humanized_id;
//    u8              alias[ALIAS_LEN + 1];
//    u8              suin;
//    bool            enabled;
//    bool            remote_control;
//    u16             puin;
//    DbObjectCode    ptype;

//    ParentDevice   *parent_device;

//    // state
//    PerformerUnitState state;
//    BehaviorPreset     behavior_preset;
//} Buzzer;

////
//typedef struct _Buzzer_tbl {
//    u16   size;
//    Buzzer *t;
//} Buzzer_tbl;

////
//s32 createRuntimeTable_Buzzers(void);
//s32 destroyRuntimeTable_Buzzers(void);
//Buzzer *getBuzzerByID(u16 buzzer_id);
//Buzzer_tbl *getBuzzerTable(s32 *error);

//Buzzer *getBuzzerByParent(u16 puin, DbObjectCode ptype, u16 suin);
//s32 setBuzzerUnitState(Buzzer *pBuzzer, PerformerUnitState state, BehaviorPreset *pBehavior);


/************************************************************************
 * BUTTONS
 ************************************************************************/
typedef struct _Button {
    u16             id;
    u16             humanized_id;
    u8              alias[ALIAS_LEN + 1];
    u8              suin;
    bool            enabled;
    u16             puin;
    DbObjectCode    ptype;

    ParentDevice   *parent_device;
    Event          *events[3];
} Button;

//
typedef struct _Button_tbl {
    u16     size;
    Button *t;
} Button_tbl;

//
s32 createRuntimeTable_Buttons(void);
s32 destroyRuntimeTable_Buttons(void);
Button *getButtonByID(u16 button_id);
Button_tbl *getButtonTable(s32 *error);

Button *getButtonByParent(u16 puin, DbObjectCode ptype, u16 suin);


//************************************************************************
//* PHONE NUMBERS
//************************************************************************/
typedef char PhoneNumberData[PHONE_LEN + 1];

typedef struct _PhoneNumber {
    u8              id;
    PhoneNumberData number;
    u8              allowed_simcard;
} PhoneNumber;

typedef struct _PhoneList {
    u8           size;
    PhoneNumber *phones;
    u8           _size_m;
    PhoneNumber *_phones_m;
} PhoneList;

s32 createRuntimeArray_Phones(SIMSlot sim);
s32 destroyRuntimeArray_Phones(SIMSlot sim);
PhoneNumber *getPhonesArray(SIMSlot sim, s32 *error);

s32 createRuntimeArray_AuxPhones(void);
s32 destroyRuntimeArray_AuxPhones(void);
PhoneNumber *getAuxPhonesArray(s32 *error);


//************************************************************************
//* IP ADDRESSES
//************************************************************************/
typedef u8 IpAddressData[IPADDR_LEN];

typedef struct _IpAddress {
    u8            id;
    IpAddressData address;
} IpAddress;

typedef struct _IpList {
    u16        dest_port;
    u16        local_port;
    u8         size;
    IpAddress *IPs;
    u8         _size_m;
    IpAddress *_IPs_m;
} IpList;

s32 createRuntimeArray_IPs(SIMSlot sim);
s32 destroyRuntimeArray_IPs(SIMSlot sim);
IpAddress *getIPsArray(SIMSlot sim, s32 *error);

bool isIPAddressValid(IpAddress *addr);


//************************************************************************
//* SIM CARDS
//************************************************************************/
typedef struct _SimSettings {
    u8  id;

    bool      can_be_used;
    bool      prefer_gprs;
    bool      allow_sms_to_pult;
    // voicecall
    u8        voicecall_attempts_qty;
    PhoneList phone_list;
    // gprs
    char      apn[APN_LEN + 1];                // +1 for '\0'
    char      login[CREDENTIALS_LEN + 1];
    char      password[CREDENTIALS_LEN + 1];
    u8        gprs_attempts_qty;
    IpList    udp_ip_list;
    IpList    tcp_ip_list;
} SimSettings;

s32 createRuntimeArray_SimSettings(void);
s32 destroyRuntimeArray_SimSettings(void);
SimSettings *getSimSettingsArray(s32 *error);

// functions below can be used only after the DB is initialized successfully
char *getPhoneByIndex(SIMSlot sim, u8 phone_index);
s16 findPhoneIndex(const char *phone, SIMSlot *simcard);

u8 *getIpAddressByIndex(SIMSlot sim, u8 IpAddress_index);
s16 findIpAddressIndex(const u8 *ip, SIMSlot *simcard);


//************************************************************************
//* COMMON SETTINGS
//************************************************************************/
typedef struct _CommonSettings {
    PhoneList            aux_phone_list;
    PultMessageCodecType codec_type;
    u16                  entry_delay_sec;
    u16                  arming_delay_sec;
    u8                   debug_level;
    u8                   aux_phohelist_default_size;

    ParentDevice *p_masterboard;

    bool bMainBellOk;
    bool bSystemTrouble;

    // DTMF signal parameters
    u16 dtmf_pulse_ms;
    u16 dtmf_pause_ms;

    // ADC
    float adc_a;
    float adc_b;
} CommonSettings;

//
s32 createRuntimeObject_CommonSettings(void);
s32 destroyRuntimeObject_CommonSettings(void);
CommonSettings *getCommonSettings(s32 *error);
u8 debugLevel(void);

// the functions below can be used only after the DB is initialized successfully
char *getAuxPhoneByIndex(u8 aux_phone_index);
s32 updateMasterboardUin(u16 uin);


/************************************************************************
 * PULT_MESSAGE BUILDER
 ************************************************************************/
typedef enum _PultMessageType {
    PMT_Normal      = 19,   // 18
    PMT_Additional  = 99    // 98
} PultMessageType;

typedef enum _PultMessageQualifier {
    PMQ_NewMsgOrSetDisarmed = 1,
    PMQ_RestoralOrSetArmed  = 3,
    PMQ_RepeatPreviouslySentMsg   = 6,
    PMQ_AuxInfoMsg          = 7
} PultMessageQualifier;

typedef struct _PultMessage {
    /* according to Contact ID */
    u16                  account;           // account <=> object's phone (or similar) number
    PultMessageType      msg_type;          // message type (see above)
    PultMessageQualifier msg_qualifier;     // 1 - new msg or set to DISARMED; 3 - new restoral or set to ARMED; 6 - repeat previous state msg
    PultMessageCode      msg_code;          // describe the event
    u16                  group_id;          // arming group
    u16                  identifier;        // User_ID or Zone_ID
    /* additional fields for Armor */
    bool  bComplex;                           //
    u8   *complex_msg_part;                   //
    u16   part_len;
    u8    frameNo;
    u8    frameTotal;
} PultMessage;


typedef struct _PultMessageFIFO PultMessageFIFO;
typedef struct _GprsPeer GprsPeer;

typedef struct _PultMessageBuilder {
    s32 (*send)(void);
    s32 (*readUdpDatagram)(GprsPeer *peer);
    s32 (*readDtmfCode)(u8 dtmfCode);

    PultMessageCodec  *codec;
    PultMessageFIFO   *txFifo;
} PultMessageBuilder;


s32 createRuntimeObject_PultMessageBuilder(void);
s32 destroyRuntimeObject_PultMessageBuilder(void);
PultMessageBuilder *getMessageBuilder(s32 *error);


/************************************************************************
 * SYSTEM INFO
 ************************************************************************/
typedef struct _SystemInfo {
    // common
    u32  settings_signature;
    char db_structure_version[SYSTEM_INFO_STRINGS_LEN + 1];
    u8   firmware_version_major;
    u8   firmware_version_minor;
    // autosync
    u32  settings_signature_autosync;
} SystemInfo;

s32 createRuntimeObject_SystemInfo(void);
s32 destroyRuntimeObject_SystemInfo(void);
s32 updateRuntimeObject_SystemInfo(void);
SystemInfo *getSystemInfo(s32 *error);
bool checkDBStructureVersion(void);


/************************************************************************
 * COMMON AUTOSYNC SETTINGS
 ************************************************************************/
typedef struct _CommonAutosyncSettings {
    u8  __blank__;
} CommonAutosyncSettings;



//************************************************************************
//* структкра для энергонезависимого сохранения состояния групп
//************************************************************************/
typedef struct _GroupStateFile {
    u8              id;
    u8              state;
} GroupStateFile;

typedef struct _GroupStateFileList {
    u8           size;
    GroupStateFile *t;
} GroupStateFileList;

s32 createRuntimeTable_GroupStateFile();
s32 destroyRuntimeTable_GroupStateFile();

GroupStateFileList *getGroupStateFile(s32 *error);






#endif /* DB_H_ */
