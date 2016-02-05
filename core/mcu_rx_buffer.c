#include "mcu_rx_buffer.h"
#include "common/debug_macro.h"
#include "core/uart.h"


#define _SIZE 2048


static u8 _buffer[_SIZE] = {0};
static s16 _free_size = _SIZE;
static s16 _size = 0;
static u16 _head = 0, _tail = 0, _read = 0;
static bool _flipped = FALSE;


static u8 *nextTailCell(void)
{
    u8 *ret = NULL;

    if (_flipped) {
        if (_tail + 1 < _head)
            ret = &_buffer[++_tail];
        else
            return NULL;   // buffer overflow
    } else {
        if (_tail + 1 < _SIZE)
            ret = &_buffer[++_tail];
        else {
            ret = &_buffer[_tail=0];
            _flipped = TRUE;
        }
    }

    ++_size;
    --_free_size;
    return ret;
}

static u8 *nextHeadCell(void)
{
    u8 *ret = NULL;

    if (_flipped) {
        if (_head + 1 < _SIZE)
            ret = &_buffer[++_head];
        else {
            ret = &_buffer[_head=0];
            _flipped = FALSE;
        }
    } else {
        if (_head < _tail)
            ret = &_buffer[++_head];
        else
            return NULL;   // buffer is empty
    }

    --_size;
    ++_free_size;
    return ret;
}

static u8 *nextReadCell(void)
{
    if (_read + 1 < _SIZE)
        ++_read;
    else
        _read = 0;

    return &_buffer[_read];
}

static bool isEmpty(void)
{ return _head == _tail; }

static s16 size(void)
{ return _size; }

static bool enqueue(u8 *data, u16 len)
{
    bool success = TRUE;

    if (_free_size < len)
        success = FALSE;

#ifndef NO_CB

    OUT_DEBUG_2("AUX CircularBuffer::enqueue(len: %d) = %s\r\n", len, success ? "TRUE" : "FALSE");

#endif

    if (success) {
        for (u16 i = 0; i < len; ++i) {
            _buffer[_tail] = data[i];
            nextTailCell();
        }
    }

    return success;
}

static s16 skip(u16 len)
{
    s16 temp = size();
    for (u16 i = 0; i < len; ++i)
        nextHeadCell();
    s16 skipped = temp - size();

#ifndef NO_CB

    OUT_DEBUG_3("AUX CircularBuffer::skip(): skipped %d bytes\r\n", skipped);

#endif
    return skipped;
}

static bool isPktAvailable(u16 *len)
{
    *len = 0;   // reset available len

    u8 *d = &_buffer[_head];
    s16 skipped = 0;

    while (size() >= MCUPKT_MIN_PKT_LEN) {    // find next pkt marker
        if (MCU_PACKET_MARKER == *d)
            break;
        d = nextHeadCell();
        ++skipped;
    }

    if (skipped)

#ifndef NO_CB

        OUT_DEBUG_3("AUX CircularBuffer::isPktAvailable(): skipped %d bytes\r\n", skipped);

#endif
    if (size() < MCUPKT_MIN_PKT_LEN)
        return FALSE;

    s16 _it = _head;
    for (s16 i = 0; i < MCUPKT_DATABLOCK_LENGTH; ++i)    // find the datablock length
        d = &_buffer[(_it+1 < _SIZE) ? (++_it) : (_it=0)];

    u16 needed_len = *d + (MCUPKT_BEGIN_DATABLOCK - 3) + 1; // evaluate entire pkt length

    /* ignore this pkt marker if ubnormal pkt len found */
    if (needed_len > MCU_MESSAGE_MAX_SIZE ||
            needed_len < MCUPKT_MIN_PKT_LEN)
    {
        skip(1);
        return FALSE;
    }

    *len = needed_len;
    return needed_len <= size();
}

static bool readPkt(u8 *buffer, u16 *len)
{
    *len = 0;   // reset result len

    bool success = TRUE;
    u16  pktlen = 0; // available len

    if (!isPktAvailable(&pktlen))
        success = FALSE;

#ifndef NO_CB

    OUT_DEBUG_2("AUX CircularBuffer::readPkt(len: %d) = %s\r\n", pktlen, success ? "TRUE" : "FALSE");

#endif

    if (success) {
        u8 *d = &_buffer[_read=_head];  // sync the read pointer

        for (u16 i = 0; i < pktlen; ++i) {
            buffer[i] = *d;
            d = nextReadCell();
            ++*len;
        }
    }

    return success;
}

CircularBuffer mcu_rx_circular_buffer = {&isEmpty,
                                         &enqueue,
                                         &isPktAvailable,
                                         &size,
                                         &skip,
                                         &readPkt};
