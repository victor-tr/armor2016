#ifndef GPRS_UDP_H
#define GPRS_UDP_H

#include "ql_type.h"
#include "pult/connection.h"
#include "common/defines.h"


typedef enum {
    GPRS_CHANNEL_OK          = 0,
    GPRS_CHANNEL_WOULDBLOCK  = -1,
    GPRS_CHANNEL_ERROR       = -3
} Enum_GPRS_CHANNEL_Error;


typedef struct _GprsPeer {
    u8  peer_ip[IPADDR_LEN];
    u16 peer_port;
    u8  rx_datagram[GPRS_RX_BUFFER_SIZE];
    u16 rx_len;
} GprsPeer;


s32  preinitGprs(void);

bool isGprsReady(void);
//GprsPeer *getGprsPeer(void);
s32 getGprsLastError(void);   // returns Enum_GPRS_CHANNEL_Error

s32  fa_activateGprs(void);    // returns Enum_GPRS_CHANNEL_Error
void deactivateGprs(void);
void activateGprs_timerHandler(void *data);

s32 sendByUDP(u8 *data, u16 len);


#endif // GPRS_UDP_H
