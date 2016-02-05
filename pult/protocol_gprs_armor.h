#ifndef PROTOCOL_GPRS_ARMOR_H
#define PROTOCOL_GPRS_ARMOR_H

#include "ql_type.h"
#include "db/db.h"


#define ARMOR_COMMON_GPRS_UDP_TIMEOUT  5000 // ms


bool isGprsAckQueued_armor(u8 *ackId, bool *repeatSending);
void init_pult_protocol_udp_armor(PultMessageBuilder *pBuilder);


#endif // PROTOCOL_GPRS_ARMOR_H
