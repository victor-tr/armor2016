#ifndef MESSAGE_CODES_H
#define MESSAGE_CODES_H


#define ENC_CODEC_TBL_SIZE   800


typedef enum _PultMessageCode {
    // Armor related (7XX) codes
    PMC_ArmorAck      = 1, // entire code is 001
    PMC_CommandReject = 2,

    PMC_ConnectionTest       = 700,

    PMC_Request_DeviceInfo   = 701,
    PMC_Response_DeviceInfo  = 751,
    PMC_Request_DeviceState  = 702,
    PMC_Response_DeviceState = 752,
    PMC_Request_LoopsState   = 703,
    PMC_Response_LoopsState  = 753,
    PMC_Request_GroupsState  = 704,
    PMC_Response_GroupsState = 754,
    PMC_Request_ReadTable    = 705,
    PMC_Response_ReadTable   = 755,
    PMC_Request_SaveTableSegment  = 706,
    PMC_Response_SaveTableSegment = 756,

    PMC_Request_VoiceCall = 411,

    PMC_Request_Apply  = 707,
    PMC_Request_Cancel = 708,

    PMC_Request_ArmedGroup  = 710,          // my
    PMC_Request_DisarmedGroup  = 711,       // my

    PMC_Request_BatteryVoltage  = 712,      // my
    PMC_Response_BatteryVoltage  = 762,     // my


    PMC_Request_PultSettingsData  = 798,
    PMC_Response_PultSettingsData = 799,


    // Fire
    PMC_Fire        = 110,
    PMC_Smoke       = 111,
    PMC_Combustion  = 112,
    PMC_WaterFlow   = 113,
    PMC_Heat        = 114,
    PMC_PullStation = 115,
    PMC_Flame       = 117,

    // Panic
    PMC_Panic       = 120,
    PMC_Duress      = 121,
    PMC_Silent      = 122,
    PMC_Audible     = 123,
    PMC_Duress_AccessGranted = 124,
    PMC_Duress_EgressGranted = 125,

    // Burglar
    PMC_Burglary    = 130,
    PMC_Perimeter   = 131,
    PMC_Interior    = 132,
    PMC_24Hour      = 133,
    PMC_Entry_Exit  = 134,
    PMC_Day_Night   = 135,
    PMC_Outdoor     = 136,
    PMC_Tamper      = 137,

    // General
    PMC_PollingLoop_Open       = 141,
    PMC_PollingLoop_Short      = 142,
    PMC_SensorTamper           = 144,
    PMC_ExpansionModuleTamper  = 145,
    PMC_PollingLoop_Fitting    = 148,
    PMC_KeyInStopList          = 149,  //

    // 24 Hour Non-Burglary
    PMC_GasDetection    = 151,
    PMC_WaterLeakage    = 154,




    // Troubles
    PMC_SystemTrouble   = 300,
    PMC_AC_Loss         = 301,
    PMC_LowSystemBattery     = 302, // system battery critical low power
    PMC_BatteryMissingOrDead = 311,
    PMC_EngineerReset   = 313,

    PMC_ProtectionLoopTrouble  = 370,

    // Peripheral troubles
    PMC_ExpansionModuleFailure = 333,   // XYZ-code GG-type CCC-uin
    PMC_ExpansionModuleReset   = 339,

    // Open/Close
    PMC_OpenClose           = 400,
    PMC_Remote_ArmDisarm    = 407,
    PMC_QuickArm            = 408,
    PMC_FailedToOpen        = 453,
    PMC_FailedToClose       = 454,
    PMC_UserOnPremise       = 458,  // begin disarming

    // tests
    PMC_ManualTriggerTestReport = 601,
    PMC_PeriodicTestReport      = 602,

    // -- custom
    PMC_SystemPowerOn           = 660,
    PMC_CANBusFreeze            = 661,

    PMC_SystemDuplicateModule   = 662


} PultMessageCode;


#endif // MESSAGE_CODES_H
