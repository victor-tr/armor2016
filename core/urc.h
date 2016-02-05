#ifndef URC_H
#define URC_H

#include "ril.h"


void RIL_URC_Handler(u32 type, u32 data);


//PST_URC OnURCHandler_RecvDTMF(const char* strURC, /*[out]*/PST_URC ptrUrc);
void OnURCHandler_RecvDTMF(const char* strURC, /*[out]*/void* reserved);
//PST_URC OnURCHandler_Call_Answered(const char* strURC, /*[out]*/PST_URC ptrUrc);
void OnURCHandler_Call_Answered(const char* strURC, /*[out]*/void* reserved);


#endif // URC_H
