#ifndef PULT_RX_BUFFER_H
#define PULT_RX_BUFFER_H

#include "common/fifo_macro.h"
#include "db/db.h"

typedef PultMessage PultMessageRx;
REGISTER_FIFO_H(PultMessageRx, pultRxBuffer)


bool Ar_PultBufferRx_isEntireMessageAvailable(void);
s32 Ar_PultBufferRx_appendMsgFrame(PultMessageRx *frame);

s32 processPultMessageRx(PultMessageRx *pMsg);

#endif // PULT_RX_BUFFER_H
