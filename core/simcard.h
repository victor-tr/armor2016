#ifndef SIMCARD_H
#define SIMCARD_H

#include "ql_type.h"
#include "pult/connection.h"
#include "db/db.h"


//
void Ar_SIM_registerCallbacks(void);

//
SimSettings *Ar_SIM_currentSettings(void);
SIMSlot Ar_SIM_currentSlot(void);
SIMSlot Ar_SIM_newSlot(void);

//
void Ar_SIM_ActivateSlot(SIMSlot simcard);
void Ar_SIM_activateSlot_timerHandler(void *data);
bool Ar_SIM_isSlotActivationFailed(void);
bool Ar_SIM_isSimCardReady(void);

//
bool Ar_SIM_canSlotBeUsed(SIMSlot simcard);
void Ar_SIM_markSimCardFailed(SIMSlot simcard);
void Ar_SIM_blockSlot(SIMSlot simcard);


#endif // SIMCARD_H
