#ifndef SYSTEM_H
#define SYSTEM_H

#include "ql_type.h"

#include "common/defines.h"

#include "core/uart.h"
#include "core/mcudefinitions.h"

#include "pult/connection.h"


enum ARMOR_SYS_MESSAGE {
    MSG_ID_DTMF_CODE_RECEIVED  = MSG_ID_USER_START + 0x100,
    MSG_ID_START_SINGLE_TIMER,
    MSG_ID_START_CONTINUOUS_TIMER,
    MSG_ID_STOP_TIMER,
    MSG_ID_VOICECALL_INTERRUPTED,
    MSG_ID_VOICECALL_INCOMING_RING,
    MSG_ID_GPRS_RX
};


typedef enum _SystemFlag {
    SysF_DeviceInitialized    = 0x01,   // initialization process finished OK
    SysF_CanHandleMcuMessages = 0x02,   // a message from MCU can be handled
    SysF_DeviceConfiguring    = 0x04    // the device is in configuring state now
} SystemFlag;

void Ar_System_setFlag(SystemFlag flag, bool state);
bool Ar_System_checkFlag(SystemFlag flag);
void Ar_System_resetFlags(void);

u32 Ar_System_readSizeAllFiles();
void Ar_System_writeSizeAllFiles(u32 add);

//
bool Ar_System_isSimSlotSwitchingAllowed(void);
void Ar_System_blockHandlingMCUMessages(bool block);

//
void Ar_System_StartDeviceInitialization(void);
void System_startupInit_timerHandler(void *data);

//
s32 Ar_System_sendEventPowerON(void);

void Ar_System_markUnitAsConnected(UnitIdent *unit);

s32 Ar_System_WDT(u8 sec);
s32 Ar_System_requestHardRestartMe(void);
s32 Ar_System_requestMcuRestart(void);
s32 Ar_System_aboutToPowerOff(u8 reason);
s32 Ar_System_enableBellAutoreport(UnitIdent *unit, bool enable);
s32 Ar_System_acceptUnitConnectionRequest(UnitIdent *unit, bool accept);
s32 Ar_System_indicateInitializationProgress(bool isActive);

//
void Ar_System_setZoneAlarmedFlag(Zone *pZone, bool enabled);

//
void Ar_System_processSoftwareTimers(void *data);

//
void Ar_System_errorBeepEtrBuzzer(u8 pulse_len, u8 pause_len, u8 nPulses);
void Ar_System_BeepEtrBuzzerAndSystemLED( ParentDevice *pParent, u8 pulse_len, u8 pause_len, u8 nPulses);


//
void Ar_System_startEtrArmedLED(ArmingGroup *pAG, u8 pulse_len, u8 pause_len, u8 nPulses);

s32 Ar_System_stopPrevETRBuzzer(ArmingGroup *pAG);
void Ar_System_startEtrBuzzer(ArmingGroup *pAG, u8 pulse_len, u8 pause_len, u8 nPulses, bool bIncrementCounter);

s32 Ar_System_stopPrevETRLed(ArmingGroup *pAG);
s32 Ar_System_setPrevETRLed(ArmingGroup *pAG, PerformerUnitState state);
bool Ar_System_isZonesInGroupNormal_Besides_WALK_THROUGH(ArmingGroup *pAG);
bool Ar_System_isZonesInGroupNormal(ArmingGroup *pAG);

void Ar_System_setBellState(PerformerUnitState state);

//
s32 Ar_System_sendEventDuplicateModule(u16 uin, DbObjectCode type);


typedef enum _SystemErrorCode {
    SEC_ClearLastError,
    SEC_Error1,
    SEC_Error2_battery_dead,
    SEC_Error3,
    SEC_Error4_no_simcard,
    SEC_Error5_sbrk
} SystemErrorCode;

void Ar_System_setSystemError(SystemErrorCode code);
void StopSystemErrorBufferCicl(void);
void StartSystemErrorBufferCicl(void);
void setFlagSystemErrorCodeInBuffer(SystemErrorCode  code);
void resetFlagSystemErrorCodeInBuffer(SystemErrorCode  code);

void Ar_System_setFOTA_process_flag(bool value);
bool Ar_System_isFOTA_process_flag(void);

void Ar_System_setAudio_flag(bool value);
bool Ar_System_isAudio_flag(void);

void Ar_System_setLastVoltage(float value);
float Ar_System_getLastVoltage(void);

void Ar_System_setPSSFreezeCounter(u16 value);
u16 Ar_System_getPSSFreezeCounter(void);



s32 Ar_System_sendEventPowerONFromBattery(void);
#endif // SYSTEM_H
