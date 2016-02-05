#ifndef POWER_H
#define POWER_H

#include "ql_type.h"


s32 Ar_Power_processPowerSourceMsg(u8 *pkt);

void Ar_Power_registerADC(void);
void Ar_Power_enableBatterySampling(bool enable);

void Ar_Power_requestAdcData(void);
s32  Ar_Power_sendAdcResponse(u16 adcValue);

bool Ar_Power_startFlag(void);
void Ar_power_updateEtrIndication(void);
#endif // POWER_H
