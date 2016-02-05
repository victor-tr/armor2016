#ifndef PROTOCOL_BASE_ARMOR_H
#define PROTOCOL_BASE_ARMOR_H

#include "ql_type.h"
#include "db/db.h"


#define ARMOR_DTMF_ONE_SIGN_INTERVAL   700 // ms

#define ARMOR_TX_MSG_DTMF_TIMEOUT(len) (((len) + 3) * ARMOR_DTMF_ONE_SIGN_INTERVAL)
#define ARMOR_RX_DTMF_TIMEOUT          (3 * ARMOR_DTMF_ONE_SIGN_INTERVAL)


void init_pult_protocol_dtmf_armor(PultMessageBuilder *pBuilder);
u8 getAndClearDtmfCodeToSend_armor(void);


#endif // PROTOCOL_BASE_ARMOR_H
