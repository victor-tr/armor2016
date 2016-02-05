#ifndef CONFIGURATOR_TX_BUFFER_H
#define CONFIGURATOR_TX_BUFFER_H

#include "ql_type.h"
#include "common/configurator_protocol.h"
#include "configurator.h"

void fillTxBuffer_configurator(DbObjectCode file_code, bool bAppend, u8 *data, u16 len);
s32 sendBufferedPacket_configurator(void);
s32 sendBufferedPacket_configurator_manual(u8 sessionKey, u8 pktIdx, u8 repeatCounter, ConfiguratorConnectionMode ccm);
void clearTxBuffer_configurator( void );


#endif // CONFIGURATOR_TX_BUFFER_H
