#include "common/debug_macro.h"
#include "common/configurator_protocol.h"
#include "core/uart.h"


u8 evaluateMcuCRC(u8 *data)
{
    u8 crc = 0;
    u16 len = MCUPKT_PrefixLen + data[MCUPKT_DATABLOCK_LENGTH];
    u8 last_bit;
    for (u16 i = 0; i < len; ++i) {
        last_bit = (0x80 & crc) ? 1 : 0;
        crc = (crc << 1) | last_bit;
        crc += data[i];
    }

    if (!crc)   // Balashov says that his CRC can't be equal to zero
        crc = 0x01;

    return crc;
}

bool checkMcuCRC(u8 *data)
{
    u16 len = MCUPKT_PrefixLen + data[MCUPKT_DATABLOCK_LENGTH];
    bool result = evaluateMcuCRC(data) == data[len];

#ifndef NO_CB

    OUT_DEBUG_2("checkMcuCRC(): %s\r\n", result ? "OK" : "BAD");

#endif

    return result;
}

u8 evaluateConfiguratorCRC(u8 *data)
{
    s16 len = SPS_PREFIX + (data[SPS_BYTES_QTY_H] << 8 | data[SPS_BYTES_QTY_L]); // packet size without CRC-byte
    u8 crc = 0;
    u8 last_bit;
    for (u16 i = 0; i < len; ++i) {
        last_bit = (0x80 & crc) ? 1 : 0;
        crc = (crc << 1) | last_bit;
        crc += data[i];
    }

    return crc;
}

bool checkConfiguratorCRC(u8 *data)
{
    s16 len = SPS_PREFIX + (data[SPS_BYTES_QTY_H] << 8 | data[SPS_BYTES_QTY_L]); // packet size without CRC-byte
    u8 crc = 0;
    u8 last_bit;
    for (u16 i = 0; i < len; ++i) {
        last_bit = (0x80 & crc) ? 1 : 0;
        crc = (crc << 1) | last_bit;
        crc += data[i];
    }

    bool result = crc == data[len];
    OUT_DEBUG_2("checkConfiguratorCRC(): %s\r\n", result ? "OK" : "BAD");
    OUT_DEBUG_8("size: %d, evaluated CRC: %#x, received CRC: %#x\r\n", len + 1, crc, data[len]);
    return result;
}


bool checkTouchMemoryCRC(u8 *data)
{
    const u8 len = 7;   // iButton key size in bytes without CRC-byte
    u8 crc = 0x00;
    u8 counter = len;

    while (counter--) {
        crc ^= *data++;
        for (u8 i = 0; i < 8; i++)
            crc = crc & 0x01 ? (crc >> 1) ^ 0x8C : crc >> 1;
    }

    return crc == data[len];
}
