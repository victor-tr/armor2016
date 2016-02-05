#ifndef PULT_REQUESTS_H
#define PULT_REQUESTS_H

#include "ql_type.h"
#include "pult/pult_rx_buffer.h"
#include "configurator/configurator.h"
#include "db/db_serv.h"

s32 rx_handler_requestReject(PultMessageRx *pMsg);
s32 rx_handler_connectionTest(PultMessageRx *pMsg);
s32 rx_handler_deviceInfo(PultMessageRx *pMsg);
s32 rx_handler_deviceState(PultMessageRx *pMsg);
s32 rx_handler_loopsState(PultMessageRx *pMsg);
s32 rx_handler_groupsState(PultMessageRx *pMsg);
s32 rx_handler_readTable(PultMessageRx *pMsg);
s32 rx_handler_saveTableSegment(PultMessageRx *pMsg);
s32 rx_handler_requestVoiceCall(PultMessageRx *pMsg);
s32 rx_handler_settingsData(PultMessageRx *pMsg);
s32 rx_handler_armedGroup(PultMessageRx *pMsg);
s32 rx_handler_disarmedGroup(PultMessageRx *pMsg);
s32 rx_handler_batteryVoltage(PultMessageRx *pMsg);




typedef enum {
    RCE_MemoryAllocationError
} ReasonCancelExecution;

#endif // PULT_REQUESTS_H
