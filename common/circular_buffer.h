#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "ql_type.h"


typedef struct {
    bool (*isEmpty)(void);
    bool (*enqueue)(u8 *data, u16 len);
    bool (*isPktAvailable)(u16 *len);
    s16  (*size)(void);
    s16  (*skip)(u16 bytes_qty);
    bool (*readPkt)(u8 *buffer, u16 *len);
} CircularBuffer;


#endif // CIRCULAR_BUFFER_H
