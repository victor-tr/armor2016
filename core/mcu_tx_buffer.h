#ifndef MCU_TX_BUFFER_H
#define MCU_TX_BUFFER_H

#include "common/fifo_macro.h"
#include "common/defines.h"
#include "core/mcudefinitions.h"


typedef struct {
    u8 data[MCU_MESSAGE_MAX_SIZE];
    u8 len;
} TxMcuMessage;

REGISTER_FIFO_H(TxMcuMessage, mcu_tx_buffer)

void process_received_mcu_ACK(void);

s32 process_mcu_tx_buffer(void);
s32 sendToMCU(UnitIdent *unit, McuCommandCode cmd, u8 *data, u16 datalen);
s32 sendToMCU_2(McuCommandRequest *pRequest);

void mcu_cts_flag_timerHandler(void *data);


#endif // MCU_TX_BUFFER_H
