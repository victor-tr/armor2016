#ifndef LEXICON_H
#define LEXICON_H

#include "db/db.h"


//************************************************************************
//* CAN CONTROLLER
//************************************************************************/
typedef struct {
    char *CmdTurnOff;
    char *CmdTurnOn;
    char *CmdAutoReport;
    char *CmdRequestedReport;
    char *CmdCancel;
} _CAN_In;

typedef struct {
    char *EvtOff    ;
    char *EvtNormal ;
    char *EvtError  ;
    char *EvtNoLink ;
    char *EvtDone   ;
    char *EvtBusy   ;
} _CAN_Out;

typedef struct {
    _CAN_In  in;
    _CAN_Out out;
} _LexiconCanController;


//************************************************************************
//* BUTTON
//************************************************************************/
typedef struct {
    char *CmdAutoReport;
    char *CmdRequestedReport;
} _Button_In;

typedef struct {
    char *EvtPress;
    char *EvtRelease;
} _Button_Out;

typedef struct {
    _Button_In  in;
    _Button_Out out;
} _LexiconButton;


//************************************************************************
//* RELAY
//************************************************************************/
typedef struct {
    char *CmdTurnOff;
    char *CmdTurnOn;
    char *CmdMultivibrator;
    char *CmdAutoReport;
    char *CmdRequestedReport;
} _Relay_In;

typedef struct {
    char *EvtOff    ;
    char *EvtOn     ;
} _Relay_Out;

typedef struct {
    _Relay_In  in;
    _Relay_Out out;
} _LexiconRelay;


//************************************************************************
//* LED
//************************************************************************/
typedef struct {
    char *CmdTurnOff;
    char *CmdTurnOn;
    char *CmdMultivibrator;
    char *CmdSetDefault;
    char *CmdAutoReport;
    char *CmdRequestedReport;
} _Led_In;

typedef struct {
    char *EvtOff    ;
    char *EvtOn     ;
} _Led_Out;

typedef struct {
    _Led_In  in;
    _Led_Out out;
} _LexiconLed;


//************************************************************************
//* AUDIO
//************************************************************************/
typedef struct {
    char *CmdTurnOff;
    char *CmdTurnOn;
    char *CmdMultivibrator;
    char *CmdSetDefault;
    char *CmdAutoReport;
    char *CmdRequestedReport;
} _Audio_In;

typedef struct {
    char *EvtOff    ;
    char *EvtOn     ;
} _Audio_Out;

typedef struct {
    _Audio_In  in;
    _Audio_Out out;
} _LexiconAudio;



//************************************************************************
//* BELL
//************************************************************************/
typedef struct {
    char *CmdTurnOff;
    char *CmdTurnOn;
    char *CmdMultivibrator;
    char *CmdAutoReport;
    char *CmdRequestedReport;
} _Bell_In;

typedef struct {
    char *EvtOff    ;
    char *EvtOn     ;
    char *EvtNormal ;
    char *EvtNoLink ;
    char *EvtShort  ;
} _Bell_Out;

typedef struct {
    _Bell_In  in;
    _Bell_Out out;
} _LexiconBell;


//************************************************************************
//* MULTIVIBRATOR
//************************************************************************/
typedef struct {
    char *CmdStart;
} _Multivibrator_In;

typedef struct {
    _Multivibrator_In  in;
} _LexiconMultivibrator;


//************************************************************************
//* SOURCE
//************************************************************************/
typedef struct {
    char *ControlDisable;
    char *ControlEnable;
    char *ControlAutoReport;
    char *ControlRequestedReport;
} _Source_In;

typedef struct {
    char *EvtTrue;
    char *EvtFalse;
} _Source_Out;

typedef struct {
    _Source_In  in;
    _Source_Out out;
} _LexiconSource;


//************************************************************************
//* BUZZER
//************************************************************************/
typedef struct {
    char *CmdTurnOff;
    char *CmdTurnOn;
    char *CmdMultivibrator;
} _Buzzer_In;

typedef struct {
    _Buzzer_In  in;
} _LexiconBuzzer;


//************************************************************************
//* === LEXICON ===
//************************************************************************/
typedef struct {
    _LexiconCanController CAN;

    _LexiconButton button;

    _LexiconRelay relay;
    _LexiconLed   led;
    _LexiconBell  bell;

    _LexiconMultivibrator multivibrator;
    _LexiconSource source;

    _LexiconBuzzer buzzer;

    _LexiconAudio audio;
} Lexicon;



extern Lexicon lexicon;

void Ar_Lexicon_register(void);
bool Ar_Lexicon_isEqual(const char *lexicon, const char *pattern);

u8 Ar_Lexicon_getLedLexiconByState(Led *pLed, u8 *data);
u8 Ar_Lexicon_getBellLexiconByState(Bell *pBell, u8 *data);
u8 Ar_Lexicon_getRelayLexiconByState(Relay *pRelay, u8 *data);
u8 Ar_Lexicon_getEtrBuzzerLexiconByState(EtrBuzzer *pBuzzer, u8 *data);

// ------------------------------------------
// -- return the number of actually used bytes
u8 Ar_Lexicon_setMultivibrator_OneBatch(u8 * const begin, u8 pulse_len, u8 pause_len, u8 nPulses);
u8 Ar_Lexicon_setMultivibrator_Meandr(u8 * const begin, u8 pulse_len, u8 pause_len);
u8 Ar_Lexicon_setMultivibrator_Normal(u8 * const begin, u8 pulse_len, u8 pause_len,
                                      u8 nPulses_in_batch, u8 batch_delay);
// ------------------------------------------


#endif // LEXICON_H
