/*
 * uart.h
 *
 * Created: 15.10.2012 13:16:06
 *  Author: user
 */

#ifndef UART_H_
#define UART_H_

#include "ql_type.h"
#include "ql_uart.h"

#include "db/db.h"
#include "mcudefinitions.h"


UnitIdent getUnitIdent(u8 *mcuPkt);

void uart_callback_handler(Enum_SerialPort port, Enum_UARTEventType event,
                                   bool pinLevel, void *customizePara);

s32 processIncomingPeripheralMessage(u8 *msg);


#endif /* UART_H_ */
