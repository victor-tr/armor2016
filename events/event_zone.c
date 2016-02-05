/*
 * event_zone.c
 *
 * Created: 18.10.2012 16:01:26
 *  Author: user
 */

#include "event_zone.h"

#include "core/uart.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "states/armingstates.h"

#include "service/humanize.h"


s32 process_mcu_zone_msg(u8 *pkt)
{
    OUT_DEBUG_2("process_mcu_zone_msg()\r\n");

    u16 parentDevUIN = pkt[MCUPKT_PARENTDEV_UIN_H] << 8 | pkt[MCUPKT_PARENTDEV_UIN_L];
    DbObjectCode parentDevType = (DbObjectCode)pkt[MCUPKT_PARENTDEV_TYPE];
    u8 zoneSUIN = pkt[MCUPKT_SUBDEV_UIN];
    u8 *datablock = &pkt[MCUPKT_BEGIN_DATABLOCK];

//    u16 parentDevId = 0xFFFF;
    // find Zone object
//    switch (parentDevType) {
//    case OBJ_MASTERBOARD:
//    {

//        break;
//    }
//    case OBJ_ZONE_EXPANDER:
//    {
//        break;
//    }
//    default:
//        // TODO: report to the pult "UNREGISTERED DEVICE"
//        return ERR_DB_RECORD_NOT_FOUND;
//    }

    // -- find Zone object in the table
    Zone *pZone = getZoneByParent(parentDevUIN, parentDevType, zoneSUIN);
    if (!pZone) {
        OUT_DEBUG_1("getZoneByParent(puin=%d, ptype=%d, suin=%d): zone not found\r\n",
                    parentDevUIN, parentDevType, zoneSUIN);
        return ERR_DB_RECORD_NOT_FOUND;
    }

    // -- check if the Zone is enabled
    if (!pZone->enabled) {
        OUT_DEBUG_3("Zone %d is disabled\r\n", pZone->humanized_id);
        return RETURN_NO_ERRORS;
    }

    PinTypeCode io_pin = (PinTypeCode)datablock[0];

    // -- WARNING: used direct conversion from HEX ASCII code to DEC
    ZoneEventType evt = (ZoneEventType)(datablock[3] - 0x30);
    pZone->last_event_type = evt;
    if (io_pin != Pin_Output)
        return ERR_BAD_PINTYPE_FOR_COMMAND;

OUT_DEBUG_8("Unit %s #%d, zone #%d, event: %d\r\n",
              getUnitTypeByCode(parentDevType), parentDevUIN, zoneSUIN, evt);

    ArmingGroup *pGroup = getArmingGroupByID(pZone->group_id);
    if (!pGroup) {
        OUT_DEBUG_1("getArmingGroupByID(id=%d): group not found\r\n", pZone->group_id);
        return ERR_DB_RECORD_NOT_FOUND;
    }

    GroupState *state = pGroup->p_group_state;

    // --  create Event
    ZoneEvent ze;
    ze.zone_event_type = evt;
    Ql_GetLocalTime(&ze.time);
    ze.p_zone = pZone;
    ze.p_group = pGroup;
    ze.p_current_arming_state = state;

    // -- invoke Handler
    s32 ret = state->handler_zone(&ze);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("%s: handler_zone() = %d error.\r\n", state->state_name, ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

