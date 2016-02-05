#ifndef CANBUS_H
#define CANBUS_H

#include "ql_type.h"
#include "common/configurator_protocol.h"
#include "mcudefinitions.h"


typedef struct _CANPkt {
    struct _CANPkt *pNext;
    u8 data[CAN_DATA_BYTES_PER_PKT];
    u8 len;
} CANPkt;

typedef struct _PeripheryMessage {
    struct _PeripheryMessage *pPrev;
    struct _PeripheryMessage *pNext;

    UnitIdent      unit;
    McuCommandCode command;

    u8      total_data_len;
    u8      last_pkt_id;
    CANPkt *pkt_chain_head;
    CANPkt *pkt_chain_tail;
} PeripheryMessage;


typedef struct _PeripheryMessageBuffer {
    PeripheryMessage *pFirstMsg;
    PeripheryMessage *pLastMsg;
    u16               buffer_size;
} PeripheryMessageBuffer;


typedef enum {
    CAN_Tx_Standby,
    CAN_Tx_ReadyToSend,
    CAN_Tx_ReadyToRepeat,
    CAN_Tx_WaitForCANACK
} CAN_TxBufferState;


void Ar_CAN_checkBusActivity(void);

typedef s32 (*PeripheryMsgHandler)(u8 *pkt);
void Ar_CAN_registerMessageHandler(PeripheryMsgHandler handler_callback);

s32 Ar_CAN_saveReceivedPkt(u8 *pkt);

s32 Ar_CAN_prepare_message_to_send(UnitIdent *pUnit, McuCommandCode cmd, u8 *data, u16 datalen);
s32 Ar_CAN_send_message(void);

void Ar_CAN_processSendingResult(CAN_TxBufferState result);
bool Ar_CAN_isTxBufferReadyToSend(void);


#endif // CANBUS_H
