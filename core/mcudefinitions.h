#ifndef MCUDEFINITIONS_H
#define MCUDEFINITIONS_H

#include "ql_type.h"
#include "common/configurator_protocol.h"


// MCU packet's HEAD structure
enum {
    MCUPKT_MARKER             = 0,
    MCUPKT_PARENTDEV_UIN_H    = 1,
    MCUPKT_PARENTDEV_UIN_L    = 2,
    MCUPKT_PARENTDEV_TYPE     = 3,
    MCUPKT_PKT_NUMBER         = 4,
    MCUPKT_DATABLOCK_LENGTH   = 5,

    MCUPKT_PrefixLen          = 6,

    MCUPKT_COMMAND_CODE       = MCUPKT_PrefixLen,
    MCUPKT_SUBDEV_UIN         = 7,
    MCUPKT_SUBDEV_TYPE        = 8,
    MCUPKT_BEGIN_DATABLOCK    = 9,

    MCUPKT_MIN_PKT_LEN  = MCUPKT_BEGIN_DATABLOCK + 1
};

// command codes
typedef enum _McuCommandCode {
    CMD_ConnectionRequest = 0x27,
    CMD_AcceptConnectionRequest = 0xA7,
    CMD_DenyConnectionRequest = 0xA8,

    CMD_GetInfo           = 0x01,
    MSG_InfoReport        = 0x81,

    CMD_GetValue          = 0x06,
    MSG_GetValue          = 0x86,

    CMD_SetValue          = 0x05,

    CMD_ReadStructure     = 0x3F,
    MSG_ReadStructure     = 0xBF,

    CMD_WriteLexicon      = 0x3A,

    CMD_Restart           = 0x16,
    CMD_FullSystemRestart = 0x18,

    CMD_GetStatus         = 0x02,
    MSG_GetStatus         = 0x82,

    CMD_ReprogramUIN      = 0x21,
    MSG_ReprogramComplete = 0xA2,
    MSG_WrongParameter    = 0xFB,

    MSG_PackReceived      = 0xA5

} McuCommandCode;

// identifies any Unit that can be addressed
typedef struct _UnitIdent {
    u16          uin;
    DbObjectCode type;
    u8           suin;
    DbObjectCode stype;
} UnitIdent;


typedef struct {
    UnitIdent unit;
    McuCommandCode cmd;
    u8 data[MCU_MESSAGE_MAX_SIZE - MCUPKT_MIN_PKT_LEN];
    u16 datalen;
} McuCommandRequest;

typedef struct {
    u8              marker;
    UnitIdent       unit;
    u8              pkt_id;
    u8              datalen;
    McuCommandCode  cmd;
    PinTypeCode     pin;
    u8             *data;
} RxMcuMessage;



#define CREATE_UNIT_IDENT(unitName, _ptype, _puin, _stype, _suin) \
    UnitIdent unitName = {0};   \
    unitName.stype = _stype;     \
    unitName.suin = _suin;       \
    unitName.type = _ptype;      \
    unitName.uin = _puin;

#define UPDATE_UNIT_IDENT(unitObject, _ptype, _puin, _stype, _suin) \
    unitObject.stype = _stype;     \
    unitObject.suin = _suin;       \
    unitObject.type = _ptype;      \
    unitObject.uin = _puin;



#endif // MCUDEFINITIONS_H
