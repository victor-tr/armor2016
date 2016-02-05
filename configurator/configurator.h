#ifndef CONFIGURATOR_H
#define CONFIGURATOR_H

#include "ql_type.h"
#include "common/configurator_protocol.h"
#include "core/timers.h"


typedef enum {
    CCM_NotConnected,
    CCM_DirectConnection,
    CCM_RemoteModemConnection,
    CCM_RemotePultConnection
} ConfiguratorConnectionMode;

typedef struct {
    u8 marker;
    u8 sessionKey;
    u8 pktIdx;
    u8 repeatCounter;
    DbObjectCode dataType;
    bool bAppend;
    u16 datalen;
    u8 *data;
} RxConfiguratorPkt;


void Ar_Configurator_startWaitingResponse(void);
void Ar_Configurator_stopWaitingResponse(void);
void Ar_Configurator_NoResponse_timerHandler(void *data);

s32 Ar_Configurator_processSettingsData(RxConfiguratorPkt *pData, ConfiguratorConnectionMode ccm);

ConfiguratorConnectionMode Ar_Configurator_ConnectionMode(void);
bool Ar_Configurator_setConnectionMode(ConfiguratorConnectionMode newMode);

u8   Ar_Configurator_transactionKey(void);
void Ar_Configurator_setTransactionKey(u8 key);

u8   Ar_Configurator_lastRecvPktId(void);
void Ar_Configurator_setLastRecvPktId(u8 pkt_id);

void Ar_Configurator_sendACK(DbObjectCode key_type);


#endif // CONFIGURATOR_H
