#include "lexicon.h"
#include "common/debug_macro.h"
#include "core/system.h"


static char *lex000 = "000";
static char *lex001 = "001";
static char *lex002 = "002";
static char *lex003 = "003";
static char *lex004 = "004";

static char *lex128 = "128";
static char *lex255 = "255";


// -- entire lexicon of the device
Lexicon lexicon;


void Ar_Lexicon_register()
{
    OUT_DEBUG_2("Ar_Lexicon_register()\r\n");

    // -- CAN controller
    // in
    lexicon.CAN.in.CmdTurnOff         = lex000;
    lexicon.CAN.in.CmdTurnOn          = lex001;
    lexicon.CAN.in.CmdAutoReport      = lex002;
    lexicon.CAN.in.CmdRequestedReport = lex003;
    lexicon.CAN.in.CmdCancel          = lex004;
    // out
    lexicon.CAN.out.EvtOff    = lex000;
    lexicon.CAN.out.EvtNormal = lex001;
    lexicon.CAN.out.EvtError  = lex128;
    lexicon.CAN.out.EvtNoLink = lex255;
    lexicon.CAN.out.EvtDone   = lex002;
    lexicon.CAN.out.EvtBusy   = lex003;

    // -- button
    // in
    lexicon.button.in.CmdAutoReport      = lex003;
    lexicon.button.in.CmdRequestedReport = lex004;
    // out
    lexicon.button.out.EvtPress   = lex001;
    lexicon.button.out.EvtRelease = lex000;

    // -- relay
    // in
    lexicon.relay.in.CmdTurnOff         = lex000;
    lexicon.relay.in.CmdTurnOn          = lex001;
    lexicon.relay.in.CmdMultivibrator   = lex002;
    lexicon.relay.in.CmdAutoReport      = lex003;
    lexicon.relay.in.CmdRequestedReport = lex004;
    // out
    lexicon.relay.out.EvtOff    = lex000;
    lexicon.relay.out.EvtOn     = lex001;

    // -- led
    // in
    lexicon.led.in.CmdTurnOff         = lex000;
    lexicon.led.in.CmdTurnOn          = lex001;
    lexicon.led.in.CmdMultivibrator   = lex002;
    lexicon.led.in.CmdSetDefault      = lex128;
    lexicon.led.in.CmdAutoReport      = lex003;
    lexicon.led.in.CmdRequestedReport = lex004;
    // out
    lexicon.led.out.EvtOff    = lex000;
    lexicon.led.out.EvtOn     = lex001;

    // -- led
    // in
    lexicon.audio.in.CmdTurnOff         = lex000;
    lexicon.audio.in.CmdTurnOn          = lex001;
    lexicon.audio.in.CmdMultivibrator   = lex002;
    lexicon.audio.in.CmdSetDefault      = lex128;
    lexicon.audio.in.CmdAutoReport      = lex003;
    lexicon.audio.in.CmdRequestedReport = lex004;
    // out
    lexicon.audio.out.EvtOff    = lex000;
    lexicon.audio.out.EvtOn     = lex001;

    // -- bell
    // in
    lexicon.bell.in.CmdTurnOff         = lex000;
    lexicon.bell.in.CmdTurnOn          = lex001;
    lexicon.bell.in.CmdMultivibrator   = lex002;
    lexicon.bell.in.CmdAutoReport      = lex003;
    lexicon.bell.in.CmdRequestedReport = lex004;
    // out
    lexicon.bell.out.EvtOff    = lex000;
    lexicon.bell.out.EvtOn     = lex001;
    lexicon.bell.out.EvtNormal = lex002;
    lexicon.bell.out.EvtNoLink = lex003;
    lexicon.bell.out.EvtShort  = lex004;

    // -- multivibrator
    lexicon.multivibrator.in.CmdStart = lex001;

    // -- source
    // in
    lexicon.source.in.ControlDisable         = lex000;
    lexicon.source.in.ControlEnable          = lex001;
    lexicon.source.in.ControlAutoReport      = lex002;
    lexicon.source.in.ControlRequestedReport = lex003;

    // out
    lexicon.source.out.EvtFalse = lex000;
    lexicon.source.out.EvtTrue  = lex001;

    // -- etr's buzzer
    // in
    lexicon.buzzer.in.CmdTurnOff       = lex000;
    lexicon.buzzer.in.CmdTurnOn        = lex001;
    lexicon.buzzer.in.CmdMultivibrator = lex002;
}


bool Ar_Lexicon_isEqual(const char *lexicon, const char *pattern)
{
    return 0 == Ql_strcmp(lexicon, pattern);
}


u8 Ar_Lexicon_getLedLexiconByState(Led *pLed, u8 *data)
{
    u8 len = 0;

    if (pLed) {
        switch (pLed->state) {
        case UnitStateOn:
            Ql_strcpy((char *)data, lexicon.led.in.CmdTurnOn);
            len = Ql_strlen(lexicon.led.in.CmdTurnOn) + 1;  // +1 - for terminal '\0'
            break;
        case UnitStateOff:
            Ql_strcpy((char *)data, lexicon.led.in.CmdTurnOff);
            len = Ql_strlen(lexicon.led.in.CmdTurnOff) + 1;
            break;
        case UnitStateMultivibrator:
            Ql_strcpy((char *)data, lexicon.led.in.CmdMultivibrator);
            len = Ql_strlen(lexicon.led.in.CmdMultivibrator) + 1;
            len += Ar_Lexicon_setMultivibrator_Normal(&data[len],
                                              pLed->behavior_preset.pulse_len,
                                              pLed->behavior_preset.pause_len,
                                              pLed->behavior_preset.pulses_in_batch,
                                              pLed->behavior_preset.batch_pause_len);
            break;
        }
    }

    return len;
}


u8 Ar_Lexicon_getBellLexiconByState(Bell *pBell, u8 *data)
{
    u8 len = 0;

    if (pBell) {
        switch (pBell->state) {
        case UnitStateOn:
            Ql_strcpy((char *)data, lexicon.bell.in.CmdTurnOn);
            len = Ql_strlen(lexicon.bell.in.CmdTurnOn) + 1;
            break;
        case UnitStateOff:
            Ql_strcpy((char *)data, lexicon.bell.in.CmdTurnOff);
            len = Ql_strlen(lexicon.bell.in.CmdTurnOff) + 1;
            break;
        case UnitStateMultivibrator:
            Ql_strcpy((char *)data, lexicon.bell.in.CmdMultivibrator);
            len = Ql_strlen(lexicon.bell.in.CmdMultivibrator) + 1;
            len += Ar_Lexicon_setMultivibrator_Normal(&data[len],
                                              pBell->behavior_preset.pulse_len,
                                              pBell->behavior_preset.pause_len,
                                              pBell->behavior_preset.pulses_in_batch,
                                              pBell->behavior_preset.batch_pause_len);
            break;
        }
    }

    return len;
}


u8 Ar_Lexicon_getRelayLexiconByState(Relay *pRelay, u8 *data)
{
    u8 len = 0;

    if (pRelay) {
        switch (pRelay->state) {
        case UnitStateOn:
            Ql_strcpy((char *)data, lexicon.relay.in.CmdTurnOn);
            len = Ql_strlen(lexicon.relay.in.CmdTurnOn) + 1;
            break;
        case UnitStateOff:
            Ql_strcpy((char *)data, lexicon.relay.in.CmdTurnOff);
            len = Ql_strlen(lexicon.relay.in.CmdTurnOff) + 1;
            break;
        case UnitStateMultivibrator:
            Ql_strcpy((char *)data, lexicon.relay.in.CmdMultivibrator);
            len = Ql_strlen(lexicon.relay.in.CmdMultivibrator) + 1;
            len += Ar_Lexicon_setMultivibrator_Normal(&data[len],
                                              pRelay->behavior_preset.pulse_len,
                                              pRelay->behavior_preset.pause_len,
                                              pRelay->behavior_preset.pulses_in_batch,
                                              pRelay->behavior_preset.batch_pause_len);
            break;
        }
    }

    return len;
}


u8 Ar_Lexicon_getEtrBuzzerLexiconByState(EtrBuzzer *pBuzzer, u8 *data)
{
    u8 len = 0;

    if (pBuzzer) {
        switch (pBuzzer->state) {
        case UnitStateOn:
            Ql_strcpy((char *)data, lexicon.buzzer.in.CmdTurnOn);
            len = Ql_strlen(lexicon.buzzer.in.CmdTurnOn) + 1;
            break;
        case UnitStateOff:
            Ql_strcpy((char *)data, lexicon.buzzer.in.CmdTurnOff);
            len = Ql_strlen(lexicon.buzzer.in.CmdTurnOff) + 1;
            break;
        case UnitStateMultivibrator:
            Ql_strcpy((char *)data, lexicon.buzzer.in.CmdMultivibrator);
            len = Ql_strlen(lexicon.buzzer.in.CmdMultivibrator) + 1;
            len += Ar_Lexicon_setMultivibrator_Normal(&data[len],
                                              pBuzzer->behavior_preset.pulse_len,
                                              pBuzzer->behavior_preset.pause_len,
                                              pBuzzer->behavior_preset.pulses_in_batch,
                                              pBuzzer->behavior_preset.batch_pause_len);
            break;
        }
    }

    return len;
}


u8 Ar_Lexicon_setMultivibrator_OneBatch(u8 * const begin, u8 pulse_len, u8 pause_len, u8 nPulses)
{
    begin[0] = pulse_len;
    begin[1] = pause_len;
    begin[2] = (0 == nPulses) ? 1 : nPulses;
    begin[3] = 0;
    return 4;   // size of multivibrator parameters in bytes
}

u8 Ar_Lexicon_setMultivibrator_Meandr(u8 * const begin, u8 pulse_len, u8 pause_len)
{
    begin[0] = pulse_len;
    begin[1] = pause_len;
    begin[2] = 0;
    begin[3] = 0;
    return 4;   // size of multivibrator parameters in bytes
}

u8 Ar_Lexicon_setMultivibrator_Normal(u8 * const begin, u8 pulse_len, u8 pause_len,
                                      u8 nPulses_in_batch, u8 batch_delay)
{
    begin[0] = pulse_len;
    begin[1] = pause_len;
    begin[2] = nPulses_in_batch;
    begin[3] = batch_delay;
    return 4;   // size of multivibrator parameters in bytes
}
