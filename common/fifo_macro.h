#ifndef FIFO_MACRO_H
#define FIFO_MACRO_H

#include "ql_type.h"
#include "ql_memory.h"

#include "common/errors.h"
#include "common/debug_macro.h"


typedef enum {
    QueueStatus_ReadyToProcessing,
    QueueStatus_InProcessing
} QueueStatus;

//
// use this macro in .h file
//
#define REGISTER_FIFO_H(ObjType, bufferName)    \
                                                \
                                                \
typedef struct _##ObjType##FIFOItem {           \
    ObjType                      d;             \
    struct _##ObjType##FIFOItem *pNext;         \
} ObjType##FIFOItem;                            \
                                                \
typedef struct _##ObjType##FIFO {               \
    s32 (*enqueue)     (ObjType *item);         \
    s32 (*dequeue)     (ObjType *item);         \
    bool (*isEmpty)    (void);                  \
    u16 (*size)        (void);                  \
    s32 (*cloneFirst)  (ObjType *item);         \
    s32 (*removeAt)    (u16 pos);               \
    s32 (*clear)       (void);                  \
    s32 (*removeFirstN)(u16 amount);            \
    s32 (*cloneAt)     (u16 pos, ObjType *item); \
    QueueStatus (*status)(void);                 \
    void (*setStatus)  (QueueStatus status);    \
} ObjType##FIFO;                                \
                                                \
extern ObjType##FIFO bufferName;



// ===============================================================================
//
// use this macro in .c file
// the callbacks can be NULL if they don't needed;
// enqueue_callback signature: void enqueue_CallBack( void )
// cleanup_callback signature: void cleanup_CallBack(ObjType*)
//
#define REGISTER_FIFO_C(ObjType, bufferName, enqueue_CallBack, msg_cleanup_CallBack)  \
                                                                \
                                                                \
typedef ObjType##FIFOItem* ObjType##FIFOItem_ptr;				\
typedef void (*enq_CallBack)(void);             \
typedef void (*cleanup_CallBack)(ObjType*);    \
                                                                \
static ObjType##FIFOItem * volatile pHead = NULL;   \
static ObjType##FIFOItem * volatile pTail = NULL;   \
static u16 fifo_size  = 0;                                      \
static QueueStatus fifo_status = QueueStatus_ReadyToProcessing;          \
                                                                \
static enq_CallBack     bufferName##_enqueue_cb = enqueue_CallBack;      \
static cleanup_CallBack bufferName##_cleanup_cb = msg_cleanup_CallBack;  \
                                                                \
                                                                \
static bool isEmpty( void )                                     \
{ return pHead == NULL; }                                       \
                                                                \
static u16 size( void )                                         \
{ return fifo_size; }                                           \
                                                                \
static s32 enqueue( ObjType *item )                             \
{                                                               \
    OUT_DEBUG_2(#bufferName"::enqueue() new item; the buffer size will be %d\r\n", fifo_size + 1); \
                                                                \
    ObjType##FIFOItem_ptr pNew = Ql_MEM_Alloc(sizeof(ObjType##FIFOItem));	\
    if (pNew != NULL) {                                         \
        pNew->d = *item;                                        \
        pNew->pNext = NULL;                                     \
                                                                \
        if (isEmpty())                                          \
            pHead = pNew;                                       \
        else                                                    \
            pTail->pNext = pNew;                                \
                                                                \
        pTail = pNew;                                           \
        fifo_size++;                                            \
                                                                \
        if (bufferName##_enqueue_cb)                          \
            bufferName##_enqueue_cb();                          \
                                                                \
        return RETURN_NO_ERRORS;                                \
    } else {                                                    \
        OUT_DEBUG_1("Failed to get %d bytes of the HEAP memory\r\n", sizeof(ObjType##FIFOItem)); \
        return ERR_GET_NEW_MEMORY_FAILED;                       \
    }                                                           \
                                                                \
}                                                               \
                                                                \
static s32 dequeue( ObjType *item )                             \
{                                                               \
    OUT_DEBUG_2(#bufferName"::dequeue() one item.\r\n");        \
                                                                \
    if (pHead == NULL) {                                        \
        OUT_DEBUG_1(#bufferName"::dequeue(): the buffer is empty\r\n"); \
        return ERR_BUFFER_IS_EMPTY;                             \
    }                                                           \
                                                                \
    ObjType##FIFOItem_ptr pTemp;                                \
                                                                \
    *item = pHead->d;                                           \
    pTemp = pHead;                                              \
    pHead = pHead->pNext;                                       \
                                                                \
    if (pHead == NULL)                                          \
        pTail = NULL;                                           \
                                                                \
    fifo_size--;                                                \
    Ql_MEM_Free(pTemp);                                \
    return RETURN_NO_ERRORS;             \
}                                                               \
                                                                \
static s32 cloneFirst(ObjType *item)                            \
{                                                               \
    OUT_DEBUG_2(#bufferName"::cloneFirst()\r\n");               \
                                                                \
    if (pHead == NULL) {                                        \
        OUT_DEBUG_1(#bufferName"::cloneFirst(): the buffer is empty\r\n"); \
        return ERR_BUFFER_IS_EMPTY;                             \
    }                                                           \
                                                                \
    *item = pHead->d;                                           \
    return RETURN_NO_ERRORS;                                    \
}                                                               \
                                                                \
static s32 removeAt(u16 pos)                                           \
{                                                               \
    OUT_DEBUG_2(#bufferName"::removeAt(pos=%d)\r\n", pos);              \
                                                                \
    if (pHead == NULL) {                                        \
        OUT_DEBUG_1(#bufferName"::removeAt(): the buffer is empty\r\n"); \
        return ERR_BUFFER_IS_EMPTY;                             \
    }                                                           \
                                                                \
    ObjType##FIFOItem_ptr pTemp = pHead;                        \
    ObjType##FIFOItem_ptr pPrev = NULL;                        \
    for (u16 i = 0; i < pos; ++i) { pPrev = pTemp; pTemp = pTemp->pNext; }       \
    if (pHead == pTemp) pHead = pTemp->pNext;                   \
    else pPrev->pNext = pTemp->pNext;          \
                                                                \
    if (pHead == NULL)                                          \
        pTail = NULL;                                           \
                                                                \
    fifo_size--;                                                \
                                                \
    if (bufferName##_cleanup_cb)                \
        bufferName##_cleanup_cb(&pTemp->d);     \
                                                \
    Ql_MEM_Free(pTemp);                                \
    return RETURN_NO_ERRORS;             \
}                                                               \
                                                                \
static s32 clear(void)                                          \
{                                                               \
    OUT_DEBUG_2(#bufferName"::clear()\r\n");                    \
                                                                \
    while (fifo_size) {                                         \
        ObjType##FIFOItem_ptr pTemp = pHead;                            \
        pHead = pHead->pNext;                                   \
                                                                \
        if (pHead == NULL)                                      \
            pTail = NULL;                                       \
                                                                \
        fifo_size--;                                            \
                                                    \
        if (bufferName##_cleanup_cb)                \
            bufferName##_cleanup_cb(&pTemp->d);     \
                                                    \
        Ql_MEM_Free(pTemp);                                   \
    }                                                           \
                                                                \
    return RETURN_NO_ERRORS;                                    \
}                                                               \
                                                                \
static s32 removeFirstN(u16 amount)    \
{   \
    OUT_DEBUG_2(#bufferName"::removeFirstN(%d)\r\n", amount); \
    \
    if (pHead == NULL) {                                        \
        OUT_DEBUG_1(#bufferName"::removeFirstN(): the buffer is empty\r\n"); \
        return ERR_BUFFER_IS_EMPTY;                             \
    }   \
    \
    if (amount > fifo_size) {   \
        OUT_DEBUG_1(#bufferName"::removeFirstN(): out of bounds error\r\n"); \
        return ERR_OUT_OF_BUFFER_BOUNDS;                              \
    }   \
    \
    u16 counter = 0;  \
    ObjType##FIFOItem_ptr pTemp = NULL;                                \
    \
    for ( ; counter < amount; ++counter) {                              \
                                                                    \
        pTemp = pHead;                                              \
        pHead = pHead->pNext;                                       \
                                                                    \
        if (pHead == NULL)                                          \
            pTail = NULL;                                           \
                                                                    \
        fifo_size--;                                                \
                                                    \
        if (bufferName##_cleanup_cb)                \
            bufferName##_cleanup_cb(&pTemp->d);     \
                                                    \
        Ql_MEM_Free(pTemp);             \
    }   \
    \
    return counter;    \
}   \
    \
static s32 cloneAt(u16 pos, ObjType *item)    \
{   \
    OUT_DEBUG_2(#bufferName"::cloneAt(pos=%d)\r\n", pos); \
    \
    if (pHead == NULL) {                                        \
        OUT_DEBUG_1(#bufferName"::cloneAt(): the buffer is empty\r\n"); \
        return ERR_BUFFER_IS_EMPTY;                             \
    }   \
    \
    if (pos > fifo_size) {   \
        OUT_DEBUG_1(#bufferName"::cloneAt(): out of bounds error\r\n"); \
        return ERR_OUT_OF_BUFFER_BOUNDS;                              \
    }   \
    \
    ObjType##FIFOItem_ptr pTemp = pHead;    \
    \
    for (u16 i = 0; i < pos; ++i) {   \
        pTemp = pTemp->pNext;    \
    }   \
    \
    *item = pTemp->d;   \
    return RETURN_NO_ERRORS;    \
}   \
    \
static QueueStatus status(void)  \
{ return fifo_status; }   \
    \
static void setStatus(QueueStatus status)   \
{   \
    OUT_DEBUG_2(#bufferName"::setStatus(\"%s\")\r\n", QueueStatus_ReadyToProcessing == status    \
        ? "READY"  \
        : "IN PROCESSING"); \
    fifo_status = status;   \
}   \
    \
/* global buffers, accessible by all states */ \
ObjType##FIFO bufferName = {&enqueue,   \
                            &dequeue,   \
                            &isEmpty,   \
                            &size,      \
                            &cloneFirst,    \
                            &removeAt,   \
                            &clear,         \
                            &removeFirstN,  \
                            &cloneAt,   \
                            &status,    \
                            &setStatus  \
                            };


#endif // FIFO_MACRO_H
