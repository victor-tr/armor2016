/*
 * crc.h
 *
 * Created: 19.10.2012 13:36:07
 *  Author: user
 */


#ifndef CRC_H_
#define CRC_H_

#include "ql_type.h"


u8 evaluateMcuCRC(u8 *data);
bool checkMcuCRC(u8 *data);
u8 evaluateConfiguratorCRC(u8 *data);
bool checkConfiguratorCRC(u8 *data);

bool checkTouchMemoryCRC(u8 *data);


#endif /* CRC_H_ */
