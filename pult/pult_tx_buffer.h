#ifndef PULT_TX_BUFFER_H
#define PULT_TX_BUFFER_H

#include "common/fifo_macro.h"
#include "db/db.h"


REGISTER_FIFO_H(PultMessage, pultTxBuffer)


s32 reportToPult(const u16 identifier,
                 const u16 group_id,
                 const PultMessageCode msg_code,
                 const PultMessageQualifier msg_qualifier);

s32 reportToPultComplex(const u8 *data,
                        const s16 datalen,
                        const PultMessageCode msg_code,
                        const PultMessageQualifier msg_qualifier);


#endif /* PULT_TX_BUFFER_H */
