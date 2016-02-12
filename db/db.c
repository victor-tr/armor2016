#include "db.h"
#include "db_serv.h"
#include "lexicon.h"

#include "ql_stdlib.h"
#include "ql_memory.h"

#include "common/debug_macro.h"
#include "common/defines.h"
#include "common/defines.h"
#include "common/errors.h"

#include "states/armingstates.h"

#include "pult/pult_tx_buffer.h"

#include "core/mcu_tx_buffer.h"
#include "core/system.h"

#include "db/fs.h"

#include "configurator/configurator_tx_buffer.h"

#include "service/humanize.h"


// *******************************************************
// DB tables
// *******************************************************
static TouchmemoryCode_tbl        tbl_TouchmemoryCode;
static KeyboardCode_tbl           tbl_KeyboardCode;
static ArmingGroup_tbl            tbl_ArmingGroup;
static Zone_tbl                   tbl_Zone;
static Relay_tbl                  tbl_Relay;
static Led_tbl                    tbl_Led;
static Bell_tbl                   tbl_Bell;
static Button_tbl                 tbl_Button;
static ParentDevice_tbl           tbl_ParentDevices;
static Event_tbl                  tbl_Event;
static BehaviorPreset_tbl         tbl_BehaviorPreset;
static Reaction_tbl               tbl_Reaction;
static RelationETRArmingGroup_tbl tbl_RelationETRArmingGroup;

static SimSettings      *arr_SimCards;
static PhoneNumber      *arr_PhoneNumbers_SIM1;
static PhoneNumber      *arr_PhoneNumbers_SIM2;
static PhoneNumber      *arr_PhoneNumbers_Aux;
static IpAddress        *arr_IpAddresses_SIM1;
static IpAddress        *arr_IpAddresses_SIM2;

static PultMessageBuilder *obj_MessageBuilder;
static CommonSettings     *obj_CommonSettings;
static SystemInfo         *obj_SystemInfo;

static GroupStateFileList tbl_GroupStateFileList;
// *******************************************************


// -- "data" parameter must be a pointer to a structure && must have "id" field
#define BINARY_SEARCH(key, data, count, ret) \
{   \
    s32 low = 0;    \
    u16 mid = 0;    \
    s32 high = count - 1;   \
    ret = -1;  \
    \
    while (low <= high) {   \
        mid = (low + high) / 2; \
    \
        if (key == data[mid].id) {   \
            ret = mid;  \
            break;       \
        } else if (key < data[mid].id)    \
            high = mid - 1;     \
        else                \
            low = mid + 1;  \
    }  \
}


//************************************************************************
//* iBUTTON CODES
//************************************************************************/
s32 createRuntimeTable_TouchmemoryCodes(void)
{
    OUT_DEBUG_2("createRuntimeTable_TouchmemoryCodes()\r\n");

    if (tbl_TouchmemoryCode.t)
        return RETURN_NO_ERRORS;

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_TOUCHMEMORY_CODES);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_TouchmemoryCode.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_TouchmemoryCode.size = file_size / sizeof(TouchmemoryCode);

    // -- fill events array
    Event_tbl *pEventsTbl = getEventTable(&ret);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getEventTable() = %d error\r\n", ret);
        return ret;
    }

    Event *pEvt = NULL;
    for (u16 j = 0; j < pEventsTbl->size; ++j) {
        pEvt = &pEventsTbl->t[j];
        if (OBJ_TOUCHMEMORY_CODE == pEvt->emitter_type) {
            TouchmemoryCode *pCode = getTouchmemoryCodeByID(pEvt->emitter_id);
            if (!pCode) continue;
            pCode->events[0] = pEvt;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_TouchmemoryCodes(void)
{
    OUT_DEBUG_2("destroyRuntimeTable_TouchmemoryCodes()\r\n");

    if (! tbl_TouchmemoryCode.t)
        return RETURN_NO_ERRORS;

    Ql_MEM_Free(tbl_TouchmemoryCode.t);
    tbl_TouchmemoryCode.t = NULL;
    tbl_TouchmemoryCode.size = 0;

    return RETURN_NO_ERRORS;
}

TouchmemoryCode_tbl *getTouchmemoryCodeTable(s32 *error)
{
    if (error && !tbl_TouchmemoryCode.t) {
        *error = createRuntimeTable_TouchmemoryCodes();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_TouchmemoryCodes() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_TouchmemoryCode;
}

TouchmemoryCode *getTouchmemoryCodeByValue(u8 *value)
{
    bool found = FALSE;

    for (u16 pos = 0; pos < tbl_TouchmemoryCode.size; ++pos) {
        for (u8 i = 0; i < TOUCHMEMORY_MAX_KEY_SIZE; ++i) {
            if (tbl_TouchmemoryCode.t[pos].value[i] != value[i]) {
                found = FALSE;
                break;
            }
            found = TRUE;
        }
        if (found)
            return &tbl_TouchmemoryCode.t[pos];
    }

    return NULL;
}

TouchmemoryCode *getTouchmemoryCodeByID(u16 id)
{
    s32 idx = -1;
    BINARY_SEARCH(id, tbl_TouchmemoryCode.t, tbl_TouchmemoryCode.size, idx);
    return -1 == idx ? NULL : &tbl_TouchmemoryCode.t[idx];
}


//************************************************************************
//* KEYBOARD CODES
//************************************************************************/
s32 createRuntimeTable_KeyboardCodes(void)
{
    OUT_DEBUG_2("createRuntimeTable_KeyboardCodes()\r\n");

    if (tbl_KeyboardCode.t)
        return RETURN_NO_ERRORS;

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_KEYBOARD_CODES);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_KeyboardCode.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_KeyboardCode.size = file_size / sizeof(KeyboardCode);

    // -- fill events array
    Event_tbl *pEventsTbl = getEventTable(&ret);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getEventTable() = %d error\r\n", ret);
        return ret;
    }

    Event *pEvt = NULL;
    for (u16 j = 0; j < pEventsTbl->size; ++j) {
        pEvt = &pEventsTbl->t[j];
        if (OBJ_KEYBOARD_CODE == pEvt->emitter_type) {
            KeyboardCode *pCode = getKeyboardCodeByID(pEvt->emitter_id);
            if (!pCode) continue;
            pCode->events[0] = pEvt;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_KeyboardCodes(void)
{
    OUT_DEBUG_2("destroyRuntimeTable_KeyboardCodes()\r\n");

    if (! tbl_KeyboardCode.t)
        return RETURN_NO_ERRORS;

    Ql_MEM_Free(tbl_KeyboardCode.t);
    tbl_KeyboardCode.t = NULL;
    tbl_KeyboardCode.size = 0;

    return RETURN_NO_ERRORS;
}

KeyboardCode_tbl *getKeyboardCodeTable(s32 *error)
{
    if (error && !tbl_KeyboardCode.t) {
        *error = createRuntimeTable_KeyboardCodes();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_KeyboardCodes() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_KeyboardCode;
}

KeyboardCode *getKeyboardCodeByValue(u8 *value)
{
    bool found = FALSE;

    for (u16 pos = 0; pos < tbl_KeyboardCode.size; ++pos) {
        for (u8 i = 0; i < KEYBOARD_MAX_KEY_SIZE; ++i) {
            if (tbl_KeyboardCode.t[pos].value[i] != value[i]) {
                found = FALSE;
                break;
            }
            found = TRUE;
        }
        if (found)
            return &tbl_KeyboardCode.t[pos];
    }

    return NULL;
}

KeyboardCode *getKeyboardCodeByID(u16 id)
{
    s32 idx = -1;
    BINARY_SEARCH(id, tbl_KeyboardCode.t, tbl_KeyboardCode.size, idx);
    return -1 == idx ? NULL : &tbl_KeyboardCode.t[idx];
}


//************************************************************************
//* RELATIONS: ARMING GROUPS - ETRs
//************************************************************************/
s32 createRuntimeTable_RelationsETRArmingGroup(void)
{
    if (tbl_RelationETRArmingGroup.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_RelationsETRArmingGroup()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_RELATIONS_ETR_AGROUP);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_RelationETRArmingGroup.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_RelationETRArmingGroup.size = file_size / sizeof(RelationETRArmingGroup);

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_RelationsETRArmingGroup(void)
{
    if (! tbl_RelationETRArmingGroup.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_RelationsETRArmingGroup()\r\n");

    Ql_MEM_Free(tbl_RelationETRArmingGroup.t);
    tbl_RelationETRArmingGroup.t = NULL;
    tbl_RelationETRArmingGroup.size = 0;

    return RETURN_NO_ERRORS;
}

RelationETRArmingGroup_tbl *getRelationsETRArmingGroupTable(s32 *error)
{
    if (error && !tbl_RelationETRArmingGroup.t) {
        *error = createRuntimeTable_RelationsETRArmingGroup();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_RelationsETRArmingGroup() = %d error\r\n",
                        *error);
            return NULL;
        }
    }

    return &tbl_RelationETRArmingGroup;
}


//************************************************************************
//* ARMING GROUPS
//************************************************************************/
s32 createRuntimeTable_ArmingGroups(void)
{
    if (tbl_ArmingGroup.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_ArmingGroups()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_ARMING_GROUPS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_ArmingGroup.t, &file_size );
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_ArmingGroup.size = file_size / sizeof(ArmingGroup);

    Ar_GroupState_initGroups(&tbl_ArmingGroup);


    // -- fill events array
    Event_tbl *pEventsTbl = getEventTable(&ret);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getEventTable() = %d error\r\n", ret);
        return ret;
    }

    Event *pEvt = NULL;
    for (u16 j = 0; j < pEventsTbl->size; ++j) {
        pEvt = &pEventsTbl->t[j];
        if (OBJ_ARMING_GROUP == pEvt->emitter_type) {
            ArmingGroup *pGroup = getArmingGroupByID(pEvt->emitter_id);
            if (!pGroup) continue;
            pGroup->events[0] = pEvt;
        }
    }


    // -- fill related ETRs table for all groups
    for (u16 pos = 0; pos < tbl_ArmingGroup.size; ++pos) {
        ret = fillRelatedETRsTable(&tbl_ArmingGroup.t[pos]);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("fillRelatedETRsTable(group_id=%d) = %d error\r\n",
                        tbl_ArmingGroup.t[pos].id, ret);
            return ret;
        }
    }
    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_ArmingGroups(void)
{
    if (! tbl_ArmingGroup.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_ArmingGroups()\r\n");

    s32 ret = 0;

    // -- clear related ETRs table for all groups
    for (u8 pos = 0; pos < tbl_ArmingGroup.size; ++pos) {
        ret = freeRelatedETRsTable(&tbl_ArmingGroup.t[pos]);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("freeRelatedETRsTable(group_id=%d) = %d error\r\n",
                        tbl_ArmingGroup.t[pos].id, ret);
            return ret;
        }
    }

    // -- clear groups table
    Ql_MEM_Free(tbl_ArmingGroup.t);
    tbl_ArmingGroup.t = NULL;
    tbl_ArmingGroup.size = 0;

    return RETURN_NO_ERRORS;
}

ArmingGroup *getArmingGroupByID(u16 group_id)
{
    s32 idx = -1;
    BINARY_SEARCH(group_id, tbl_ArmingGroup.t, tbl_ArmingGroup.size, idx);
    return -1 == idx ? NULL : &tbl_ArmingGroup.t[idx];
}

ArmingGroup_tbl *getArmingGroupTable(s32 *error)
{
    if (error && !tbl_ArmingGroup.t) {
        *error = createRuntimeTable_ArmingGroups();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_ArmingGroups() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_ArmingGroup;
}

bool isGroupAlarmed(ArmingGroup *pGroup)
{
    Zone_tbl *zones_table = getZoneTable(NULL);

    for (u16 pos = 0; pos < zones_table->size; ++pos) {
        if (zones_table->t[pos].group_id == pGroup->id)
            continue;

        if (zones_table->t[pos].zoneDamage_counter > 0)
            return TRUE;
    }

    return FALSE;
}

s32 fillRelatedETRsTable(ArmingGroup *pGroup)
{
    if (!pGroup->related_ETRs_qty)
        return RETURN_NO_ERRORS;

    if (pGroup->tbl_related_ETRs.t)
        return ERR_DB_TABLE_ALREADY_EXISTS_IN_RAM;

    s32 ret = 0;
    RelationETRArmingGroup_tbl *relations_table = getRelationsETRArmingGroupTable(&ret);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    getParentDevicesTable(&ret);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    pGroup->tbl_related_ETRs.t = Ql_MEM_Alloc(sizeof(ParentDevice *) * pGroup->related_ETRs_qty);
    if (!pGroup->tbl_related_ETRs.t) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP for related ETRs table of group_id=%d\r\n",
                    sizeof(ParentDevice *) * pGroup->related_ETRs_qty,
                    pGroup->id);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    pGroup->tbl_related_ETRs.size = 0;

    for (u16 pos = 0; pos < relations_table->size; ++pos) {
        if (pGroup->id == relations_table->t[pos].group_id) {
            pGroup->tbl_related_ETRs.t[pGroup->tbl_related_ETRs.size++] =
                                    getParentDeviceByID(relations_table->t[pos].ETR_id, OBJ_ETR);
        }
        if (pGroup->tbl_related_ETRs.size == pGroup->related_ETRs_qty)
            break;
    }

    return RETURN_NO_ERRORS;
}

s32 freeRelatedETRsTable(ArmingGroup *pGroup)
{
    if (!pGroup->tbl_related_ETRs.t)
        return RETURN_NO_ERRORS;

    s32 ret = 0;

    Ql_MEM_Free(pGroup->tbl_related_ETRs.t);
    if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_MEM_Free() for related ETRs table of group_id=%d = %d error\r\n",
                    pGroup->id, ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}


//************************************************************************
//* TRIGGER EVENTS
//************************************************************************/
s32 createRuntimeTable_Events()
{
    if (tbl_Event.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_Events()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_EVENTS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Event.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Event.size = file_size / sizeof(Event);

    // -- fill events' subscribers tables
    for (u16 i = 0; i < tbl_Event.size; ++i) {
        ret = fillEventSubscribersTable(&tbl_Event.t[i]);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("fillEventSubscribersTable(event_id=%d) = %d error\r\n",
                        tbl_Event.t[i].id, ret);
            return ret;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Events()
{
    if (! tbl_Event.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Events()\r\n");

    s32 ret = 0;

    // -- clear event's subscribers tables
    for (u8 i = 0; i < tbl_Event.size; ++i) {
        ret = freeEventSubscribersTable(&tbl_Event.t[i]);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("freeEventSubscribersTable(event_id=%d) = %d error\r\n",
                        tbl_Event.t[i].id, ret);
            return ret;
        }
    }

    // -- clear events table
    Ql_MEM_Free(tbl_Event.t);
    tbl_Event.t = NULL;
    tbl_Event.size = 0;

    return RETURN_NO_ERRORS;
}

Event *getEventByID(u16 id)
{
    s32 idx = -1;
    BINARY_SEARCH(id, tbl_Event.t, tbl_Event.size, idx);
    return -1 == idx ? NULL : &tbl_Event.t[idx];
}

Event_tbl *getEventTable(s32 *error)
{
    if (error && !tbl_Event.t) {
        *error = createRuntimeTable_Events();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Events() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Event;
}

s32 fillEventSubscribersTable(Event *pEvent)
{
    if (0 == pEvent->subscribers_qty)
        return RETURN_NO_ERRORS;

    if (pEvent->tbl_subscribers.t)
        return ERR_DB_TABLE_ALREADY_EXISTS_IN_RAM;

    s32 ret = 0;
    Reaction_tbl *reactions_table = getReactionTable(&ret);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    pEvent->tbl_subscribers.t = Ql_MEM_Alloc(sizeof(Reaction*) * pEvent->subscribers_qty);
    if (!pEvent->tbl_subscribers.t) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP for subscribers of event_id=%d\r\n",
                    sizeof(Reaction*) * pEvent->subscribers_qty,
                    pEvent->id);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    pEvent->tbl_subscribers.size = 0;

    for (u16 pos = 0; pos < reactions_table->size; ++pos) {
        Reaction *pReaction = &reactions_table->t[pos];
        if (pEvent->id == pReaction->event_id) {
            pEvent->tbl_subscribers.t[pEvent->tbl_subscribers.size++] = pReaction;
        }
        if (pEvent->tbl_subscribers.size == pEvent->subscribers_qty)
            break;
    }

    return RETURN_NO_ERRORS;
}

s32 freeEventSubscribersTable(Event *pEvent)
{
    if (!pEvent->tbl_subscribers.t)
        return RETURN_NO_ERRORS;

    Ql_MEM_Free(pEvent->tbl_subscribers.t);
    pEvent->tbl_subscribers.size = 0;
    pEvent->tbl_subscribers.t = NULL;
    return RETURN_NO_ERRORS;
}

/* pState can be NULL if a state is undefined */
s32 processReactions(Event *pTrigger, GroupState *pState)
{
    OUT_DEBUG_2("processReactions()\r\n");

    if (!pTrigger)
        return RETURN_NO_ERRORS;

OUT_DEBUG_8("evt address: %p, subscribers table size = %d\r\n", pTrigger, pTrigger->tbl_subscribers.size);
OUT_DEBUG_8("evt_table_size: %d\r\n", getEventTable(NULL)->size);

    for (u8 j = 0; j < pTrigger->tbl_subscribers.size; ++j) {
        Reaction *pReaction = pTrigger->tbl_subscribers.t[j];

        /* skip reactions invalid by state flags */
        if (pState && !pReaction->valid_states[pState->state_ident])
            continue;

        s32 ret = 0;

        switch (pReaction->performer_type) {
        case OBJ_RELAY:
        {
            Relay *pRelay = getRelayByID(pReaction->performer_id);
            if (!pRelay) continue;
            bool useReverse = isReversibleReaction(pReaction) &&
                                pReaction->performer_behavior == pRelay->state;
            u8 performerBehavior = useReverse ? pReaction->reverse_behavior : pReaction->performer_behavior;
            u16 behaviorPresetId = useReverse ? pReaction->reverse_preset_id : pReaction->behavior_preset_id;
            BehaviorPreset *pBehaviorPreset = getBehaviorPresetByID(behaviorPresetId);
            ret = setRelayUnitState(pRelay, (PerformerUnitState)performerBehavior, pBehaviorPreset);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("setRelayUnitState() = %d error\r\n", ret);
                return ret;
            }
            break;
        }

        case OBJ_LED:
        {
            Led *pLed = getLedByID(pReaction->performer_id);
            if (!pLed) continue;
            bool useReverse = isReversibleReaction(pReaction) &&
                                pReaction->performer_behavior == pLed->state;
            u8 performerBehavior = useReverse ? pReaction->reverse_behavior : pReaction->performer_behavior;
            u16 behaviorPresetId = useReverse ? pReaction->reverse_preset_id : pReaction->behavior_preset_id;
            BehaviorPreset *pBehaviorPreset = getBehaviorPresetByID(behaviorPresetId);
            ret = setLedUnitState(pLed, (PerformerUnitState)performerBehavior, pBehaviorPreset);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("setLedUnitState() = %d error\r\n", ret);
                return ret;
            }
            break;
        }

        case OBJ_BELL:
        {
            Bell *pBell = getBellByID(pReaction->performer_id);
            if (!pBell) continue;
            bool useReverse = isReversibleReaction(pReaction) &&
                                pReaction->performer_behavior == pBell->state;
            u8 performerBehavior = useReverse ? pReaction->reverse_behavior : pReaction->performer_behavior;
            u16 behaviorPresetId = useReverse ? pReaction->reverse_preset_id : pReaction->behavior_preset_id;
            BehaviorPreset *pBehaviorPreset = getBehaviorPresetByID(behaviorPresetId);
            ret = setBellUnitState(pBell, (PerformerUnitState)performerBehavior, pBehaviorPreset);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("setBellUnitState() = %d error\r\n", ret);
                return ret;
            }
            break;
        }

        default:
            break;
        }
    }

    return RETURN_NO_ERRORS;
}


//************************************************************************
//* BEHAVIOR PRESETS
//************************************************************************/
s32 createRuntimeTable_BehaviorPresets()
{
    if (tbl_BehaviorPreset.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_BehaviorPresets()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_BEHAVIOR_PRESETS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_BehaviorPreset.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_BehaviorPreset.size = file_size / sizeof(BehaviorPreset);

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_BehaviorPresets()
{
    if (! tbl_BehaviorPreset.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_BehaviorPresets()\r\n");

    Ql_MEM_Free(tbl_BehaviorPreset.t);
    tbl_BehaviorPreset.t = NULL;
    tbl_BehaviorPreset.size = 0;

    return RETURN_NO_ERRORS;
}

BehaviorPreset *getBehaviorPresetByID(u16 id)
{
    s32 idx = -1;
    BINARY_SEARCH(id, tbl_BehaviorPreset.t, tbl_BehaviorPreset.size, idx);
    return -1 == idx ? NULL : &tbl_BehaviorPreset.t[idx];
}

BehaviorPreset_tbl *getBehaviorPresetTable(s32 *error)
{
    if (error && !tbl_BehaviorPreset.t) {
        *error = createRuntimeTable_BehaviorPresets();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_BehaviorPresets() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_BehaviorPreset;
}

bool isBehaviorPresetEqual(BehaviorPreset *first, BehaviorPreset *second)
{
    bool ok = TRUE;
    ok = ok && (first->pulse_len == second->pulse_len);
    ok = ok && (first->pause_len == second->pause_len);
    ok = ok && (first->pulses_in_batch == second->pulses_in_batch);
    ok = ok && (first->batch_pause_len == second->batch_pause_len);
    return ok;
}

//************************************************************************
//* REACTIONS
//************************************************************************/
s32 createRuntimeTable_Reactions()
{
    if (tbl_Reaction.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_Reactions()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_REACTIONS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Reaction.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Reaction.size = file_size / sizeof(Reaction);

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Reactions()
{
    if (! tbl_Reaction.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Reactions()\r\n");

    Ql_MEM_Free(tbl_Reaction.t);
    tbl_Reaction.t = NULL;
    tbl_Reaction.size = 0;

    return RETURN_NO_ERRORS;
}

Reaction *getReactionByID(u16 id)
{
    s32 idx = -1;
    BINARY_SEARCH(id, tbl_Reaction.t, tbl_Reaction.size, idx);
    return -1 == idx ? NULL : &tbl_Reaction.t[idx];
}

Reaction_tbl *getReactionTable(s32 *error)
{
    if (error && !tbl_Reaction.t) {
        *error = createRuntimeTable_Reactions();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Reactions() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Reaction;
}

bool isReversibleReaction(Reaction *pReaction)
{ return pReaction->bReversible; }


//************************************************************************
//* PARENT DEVICES
//************************************************************************/
s32 createRuntimeTable_ParentDevices(void)
{
    if (tbl_ParentDevices.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_ParentDevices()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_PARENT_DEVS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_ParentDevices.t, &file_size );
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_ParentDevices.size = file_size / sizeof(ParentDevice);


    // -- fill related groups table for all ETRs
    for (u16 pos = 0; pos < tbl_ParentDevices.size; ++pos) {
        if (OBJ_ETR != tbl_ParentDevices.t[pos].type)
            continue;

        ret = fillRelatedGroupsTable(&tbl_ParentDevices.t[pos]);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("fillRelatedGroupsTable(ETR id=%d) = %d error\r\n",
                        tbl_ParentDevices.t[pos].id, ret);
            return ret;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_ParentDevices(void)
{
    if (! tbl_ParentDevices.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_ParentDevices()\r\n");

    s32 ret = 0;

    // -- clear related groups table for all ETRs
    for (u8 pos = 0; pos < tbl_ParentDevices.size; ++pos) {
        if (OBJ_ETR != tbl_ParentDevices.t[pos].type)
            continue;

        ret = freeRelatedGroupsTable(&tbl_ParentDevices.t[pos]);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("freeRelatedGroupsTable(ETR id=%d) = %d error\r\n",
                        tbl_ParentDevices.t[pos].id, ret);
            return ret;
        }
    }

    // -- clear parent units table
    Ql_MEM_Free(tbl_ParentDevices.t);
    tbl_ParentDevices.t = NULL;
    tbl_ParentDevices.size = 0;

    return RETURN_NO_ERRORS;
}

ParentDevice *getParentDeviceByUIN(u16 uin, DbObjectCode type)
{
    for (u16 i = 0; i < tbl_ParentDevices.size; ++i) {
        if (tbl_ParentDevices.t[i].type == type &&
                (tbl_ParentDevices.t[i].uin == uin || OBJ_MASTERBOARD == type))
        {
            return &tbl_ParentDevices.t[i];
        }
    }
    return NULL;
}

ParentDevice *getParentDeviceByID(u16 id, DbObjectCode type)
{
    for (u16 i = 0; i < tbl_ParentDevices.size; ++i) {
        if (tbl_ParentDevices.t[i].type == type && tbl_ParentDevices.t[i].id == id)
            return &tbl_ParentDevices.t[i];
    }
    return NULL;
}

ParentDevice_tbl *getParentDevicesTable(s32 *error)
{
    if (error && !tbl_ParentDevices.t) {
        *error = createRuntimeTable_ParentDevices();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_ParentDevices() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_ParentDevices;
}

bool isParentDeviceConnected(u16 uin, DbObjectCode type)
{
    if (0 == type)
        return tbl_ParentDevices.size > 0;  // NOTE: >0? or >1 because masterboard always is in the table?

    if (0 == uin) {
        for (u16 i = 0; i < tbl_ParentDevices.size; ++i) {
            if (type == tbl_ParentDevices.t[i].type)
                return TRUE;
        }
        return FALSE;
    }

    ParentDevice *pParent = getParentDeviceByUIN(uin, type);
    if (pParent)
        return UnitConnected == pParent->connection_state.value ||
                UnitConnectedReported == pParent->connection_state.value;
    else
        return FALSE;
}


// ETR related functions
s32 requestLastReceivedTouchMemoryCode(u16 etrUIN)
{
    OUT_DEBUG_2("requestLastReceivedTouchMemoryCode(UIN=%d)\r\n", etrUIN);

    ParentDevice *pParent = getParentDeviceByUIN(etrUIN, OBJ_ETR);
    if (!pParent) {
        OUT_DEBUG_1("getParentDeviceByUIN(<%s #%d>) = FALSE\r\n", getUnitTypeByCode(OBJ_ETR), etrUIN);
        return ERR_DB_RECORD_NOT_FOUND;
    }

    // -- set to key waiting mode
    pParent->d.etr.bBlocked = TRUE;

    // -- show that ETR is ready to read key
    Led *pLed = getLedByParent(etrUIN, OBJ_ETR, 0x01);
    if (!pLed) return ERR_DB_RECORD_NOT_FOUND;

    BehaviorPreset pBehavior = {0};
    pBehavior.pulse_len = 2;   // infinite meandr
    pBehavior.pause_len = 2;

    return setLedUnitState(pLed, UnitStateMultivibrator, &pBehavior);
}

s32 sendLastReceivedTouchMemoryCode(u16 etrUIN, const u8 *last_key)
{
    OUT_DEBUG_2("sendLastReceivedTouchMemoryCode(UIN=%d)\r\n", etrUIN);

    if (!last_key) {
        OUT_DEBUG_1("The KEY is empty\r\n");
        return RETURN_NO_ERRORS;
    }

    u8 key[TMVPKT_MAX] = {0};

    key[TMVPKT_ETR_UIN_H] = etrUIN >> 8;
    key[TMVPKT_ETR_UIN_L] = etrUIN;
    key[TMVPKT_KEY_SIZE] = TOUCHMEMORY_MAX_KEY_SIZE;

    for (u8 i = 0; i < TOUCHMEMORY_MAX_KEY_SIZE; ++i)
        key[TMVPKT_KEY_H + i] = last_key[i];

    fillTxBuffer_configurator(PC_LAST_TOUCHMEMORY_CODE_OK, FALSE, key, sizeof(key));
    return sendBufferedPacket_configurator();
}

void clearETRsBlocking(void)
{
    OUT_DEBUG_2("clearETRsBlocking()\r\n");

    ParentDevice_tbl *parents_table = getParentDevicesTable(NULL);

    for (u16 pos = 0; pos < parents_table->size; ++pos) {
        if (OBJ_ETR != parents_table->t[pos].type)
            continue;

        if (parents_table->t[pos].d.etr.bBlocked)
            finishETRWatingKey(&parents_table->t[pos]);
    }
}

void finishETRWatingKey(ParentDevice *pParentETR)
{
    if (OBJ_ETR != pParentETR->type)
        return;

    // -- stop ETR's big led blinking
    Led *pLed = getLedByParent(pParentETR->uin, OBJ_ETR, 0x01);
    if (pLed) setLedUnitState(pLed, UnitStateOff, NULL);

    // -- disable blocking
    pParentETR->d.etr.bBlocked = FALSE;
}


s32 fillRelatedGroupsTable(ParentDevice *pParent)
{
    ETR *pEtr = &pParent->d.etr;

    if (!pEtr->related_groups_qty)
        return RETURN_NO_ERRORS;

    if (pEtr->tbl_related_groups.t)
        return ERR_DB_TABLE_ALREADY_EXISTS_IN_RAM;

    s32 ret = 0;
    RelationETRArmingGroup_tbl *relations_table = getRelationsETRArmingGroupTable(&ret);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    getArmingGroupTable(&ret);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    pEtr->tbl_related_groups.t = Ql_MEM_Alloc(sizeof(ArmingGroup *) * pEtr->related_groups_qty);
    if (!pEtr->tbl_related_groups.t) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP for related groups table of ETR_id=%d\r\n",
                    sizeof(ArmingGroup *) * pEtr->related_groups_qty,
                    pParent->id);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    pEtr->tbl_related_groups.size = 0;

    for (u16 pos = 0; pos < relations_table->size; ++pos) {
        if (pParent->id == relations_table->t[pos].ETR_id) {
            pEtr->tbl_related_groups.t[pEtr->tbl_related_groups.size++] =
                                        getArmingGroupByID(relations_table->t[pos].group_id);
        }
        if (pEtr->tbl_related_groups.size == pEtr->related_groups_qty)
            break;
    }

    return RETURN_NO_ERRORS;
}

s32 freeRelatedGroupsTable(ParentDevice *pParent)
{
    ETR *pEtr = &pParent->d.etr;

    if (!pEtr->tbl_related_groups.t)
        return RETURN_NO_ERRORS;

    s32 ret = 0;

    Ql_MEM_Free(pEtr->tbl_related_groups.t);
    if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Ql_MEM_Free() for related groups table of ETR_id=%d = %d error\r\n",
                    pParent->id, ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}


static bool isAllLedsTurnedOff(EtrLedIndicator *led_indicator, u8 decade_id)
{
    bool all_leds_disabled = TRUE;
    for (u8 i = 0; i < 10; ++i) {
        if (0x30 != led_indicator->led_decade_state_masks_low[decade_id][i]) {
            all_leds_disabled = FALSE;
            break;
        }
    }
    return all_leds_disabled;
}

void initETR(ParentDevice *pEtr)
{
    if (OBJ_ETR != pEtr->type)
        return;

    OUT_DEBUG_2("initETR(ETR UIN=%d)\r\n", pEtr->uin);

    // -- reset Zones indication
    setETRLedIndicatorDecade(pEtr, 0);

    bool b_AC_ok=Ar_Power_startFlag();
    // -- setup LEDs
    McuCommandRequest r = {{0}};

    r.cmd = CMD_SetValue;
    r.unit.stype = OBJ_LED;
    r.unit.type = OBJ_ETR;
    r.unit.uin = pEtr->uin;

    r.data[0] = Pin_Input;

    r.unit.suin = 0x02;  // Ext power LED
    Led *pLed = getLedByParent(r.unit.uin, r.unit.type, r.unit.suin);
    pLed->state = (b_AC_ok ? 1 : 0);
    r.datalen = 1 + Ar_Lexicon_getLedLexiconByState(pLed, &r.data[1]);
    sendToMCU_2(&r);

    r.unit.suin = 0x03;  // Battery LED
    pLed = getLedByParent(r.unit.uin, r.unit.type, r.unit.suin);
    pLed->state = (b_AC_ok ? 0 : 1);
    r.datalen = 1 + Ar_Lexicon_getLedLexiconByState(pLed, &r.data[1]);
    sendToMCU_2(&r);

    r.unit.suin = 0x04;  // System LED
    pLed = getLedByParent(r.unit.uin, r.unit.type, r.unit.suin);
    r.datalen = 1 + Ar_Lexicon_getLedLexiconByState(pLed, &r.data[1]);
    sendToMCU_2(&r);

    // -- setup buzzer
    r.unit.stype = OBJ_BUZZER;
    r.unit.suin = 0x01;
    EtrBuzzer *pBuzzer = &pEtr->d.etr.buzzer;
    r.datalen = 1 + Ar_Lexicon_getEtrBuzzerLexiconByState(pBuzzer,&r.data[1]);
    sendToMCU_2(&r);
}

void setETRLedIndicatorLedState(ParentDevice *pEtr, u8 zone_id, PerformerUnitState state)
{
    if (OBJ_ETR != pEtr->type || zone_id > 99 || zone_id < 1)
        return;

    OUT_DEBUG_2("setETRLedIndicatorLedState(ETR UIN=%d, zone_id=%d, unit state=%d)\r\n",
                pEtr->uin, zone_id, state);

     u8 target_decade = zone_id / 10;
     u8 target_led = zone_id % 10;

   // +++
    if(target_led == 0)
    {
        if(target_decade > 0)
        {
            target_decade = target_decade - 1;
            target_led = 10;
        }
    }

    OUT_DEBUG_2("(target_decade=%d, target_led=%d, )\r\n",
                target_decade, target_led);

    CREATE_UNIT_IDENT(unit, OBJ_ETR, pEtr->uin, OBJ_LED_INDICATOR, 0);

    EtrLedIndicator *li = &pEtr->d.etr.led_indicator;

    // -- remember led's state
    u8 led_state = 0x30;    // default is UnitStateOff
    if (UnitStateOn == state)
        led_state = 0x31;
    else if (UnitStateMultivibrator == state)
        led_state = 0x32;

    li->led_decade_state_masks_low[target_decade][target_led-1] = led_state;  // always remember


    u8 data[11] = {0};
    data[0] = Pin_Input;

    // -- send low indicator data if needed
    if (target_decade == li->current_led_decade)
    {
        unit.suin = 2; // high indicator
        Ql_memcpy(&data[1], li->led_decade_state_masks_low[target_decade], LEN_OF(data)-1);
    }
    else    // -- else send high indicator data
    {
        unit.suin = 1; // low indicator
        li->led_decade_state_mask_high[target_decade] = isAllLedsTurnedOff(li, target_decade)
                                                            ? 0x30 + UnitStateOff
                                                            : 0x30 + UnitStateMultivibrator;
        Ql_memcpy(&data[1], li->led_decade_state_mask_high, LEN_OF(data)-1);
    }

    if (isParentDeviceConnected(pEtr->uin, OBJ_ETR))   // send only if the device is presented
    {
        s32 ret = sendToMCU(&unit, CMD_SetValue, data, LEN_OF(data));
        if (ret < RETURN_NO_ERRORS)
            OUT_DEBUG_1("setETRLedIndicatorLedState(): sendToMCU(ETR UIN = %d) = %d error\r\n",
                        unit.uin, ret);
    }
}

void setETRLedIndicatorDecade(ParentDevice *pEtr, u8 new_decade_id)
{
    if (OBJ_ETR != pEtr->type || new_decade_id > 9)
        return;

    OUT_DEBUG_2("setETRLedIndicatorDecade(ETR UIN=%d, decade_id=%d)\r\n",
                pEtr->uin, new_decade_id);

    EtrLedIndicator *li = &pEtr->d.etr.led_indicator;
    const u8 previous_decade_id = li->current_led_decade;

    if (new_decade_id != previous_decade_id ||
            0 == new_decade_id)   // allow 0 for init purposes
    {
        // -- reset current decade of the led indicator before changing it
        li->led_decade_state_mask_high[previous_decade_id] = isAllLedsTurnedOff(li, previous_decade_id)
                                                        ? 0x30 + UnitStateOff
                                                        : 0x30 + UnitStateMultivibrator;

        li->current_led_decade = new_decade_id;
        li->led_decade_state_mask_high[new_decade_id] = 0x30 + UnitStateOn;

        // --------------------------------------------------------------
        u8 data[11] = {0};
        data[0] = Pin_Input;

        s32 ret = 0;

        // -- high
        CREATE_UNIT_IDENT(unit, OBJ_ETR, pEtr->uin, OBJ_LED_INDICATOR, 0x01);
        Ql_memcpy(&data[1], li->led_decade_state_mask_high, LEN_OF(data)-1);

        ret = sendToMCU(&unit, CMD_SetValue, data, LEN_OF(data));
        if (ret < RETURN_NO_ERRORS)
            OUT_DEBUG_1("set decade: sendToMCU(ETR UIN = %d) = %d error\r\n", unit.uin, ret);

        // -- low
        unit.suin = 2;
        Ql_memcpy(&data[1], li->led_decade_state_masks_low[new_decade_id], LEN_OF(data)-1);

        ret = sendToMCU(&unit, CMD_SetValue, data, LEN_OF(data));
        if (ret < RETURN_NO_ERRORS)
            OUT_DEBUG_1("set leds: sendToMCU(ETR UIN = %d) = %d error\r\n", unit.uin, ret);
        // --------------------------------------------------------------
    }
}

bool isETRRelatedToGroup(const u16 group_id, const ETR *pEtr)
{
    for (u16 pos = 0; pos < pEtr->tbl_related_groups.size; ++pos) {
        if (group_id == pEtr->tbl_related_groups.t[pos]->id)
            return TRUE;
    }
    return FALSE;
}

void batchSetETRLedIndicatorsLedState(Zone *pZone, PerformerUnitState state)
{
    ParentDevice_tbl *pParentsTable = getParentDevicesTable(NULL);

    if (pParentsTable) {
        for (u16 i = 0; i < pParentsTable->size; ++i) {
            if (OBJ_ETR != pParentsTable->t[i].type)
                continue;

            ETR *pEtr = &pParentsTable->t[i].d.etr;

            for (u16 pos = 0; pos < pEtr->tbl_related_groups.size; ++pos) {
                if (pZone->group_id == pEtr->tbl_related_groups.t[pos]->id)
                    setETRLedIndicatorLedState(&pParentsTable->t[i], pZone->humanized_id, state);
            }

        }
    }
}

s32 setEtrBuzzerUnitState(EtrBuzzer *pBuzzer, PerformerUnitState state, BehaviorPreset *pBehavior)
{
    if (pBuzzer->state == state &&
            UnitStateMultivibrator != state)
    {
        return RETURN_NO_ERRORS;
    }

    // -- remember the state
    pBuzzer->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, OBJ_ETR, pBuzzer->puin, OBJ_BUZZER, 0x01)

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pBuzzer->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.relay.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}


s32 setEtrBuzzerUnitStateALL(EtrBuzzer *pBuzzer, PerformerUnitState state, BehaviorPreset *pBehavior)
{
    // -- remember the state
    pBuzzer->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, OBJ_ETR, pBuzzer->puin, OBJ_BUZZER, 0x01)

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pBuzzer->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.relay.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}


//************************************************************************
//* ZONES
//************************************************************************/
s32 createRuntimeTable_Zones(void)
{
    if (tbl_Zone.t)
        return RETURN_NO_ERRORS;

    s32 ret = RETURN_NO_ERRORS;
    getParentDevicesTable(&ret);    // ensure that the table of parent units exists
    if (ret < RETURN_NO_ERRORS)
        return ret;

    OUT_DEBUG_2("createRuntimeTable_Zones()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_ZONES);
    ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Zone.t, &file_size );
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Zone.size = file_size / sizeof(Zone);

    // -- fill parent fields
    for (u16 i = 0; i < tbl_Zone.size; ++i)
    {
        Zone *pZone = &tbl_Zone.t[i];
        DbObjectCode type = pZone->ptype;
        u16 uin = pZone->puin;
        ParentDevice *pParent = getParentDeviceByUIN(uin, type);
        if (pParent) {
            pZone->parent_device = pParent;
        } else {
            OUT_DEBUG_1("Can't find parent %s #%d for Zone #%d\r\n",
                        getUnitTypeByCode(type), uin, pZone->suin);
            return ERR_DB_RECORD_NOT_FOUND;
        }
    }

    // -- fill events array
    Event_tbl *pEventsTbl = getEventTable(&ret);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getEventTable() = %d error\r\n", ret);
        return ret;
    }

    Event *pEvt = NULL;
    for (u16 j = 0; j < pEventsTbl->size; ++j) {
        pEvt = &pEventsTbl->t[j];
        if (OBJ_ZONE == pEvt->emitter_type) {
            Zone *pZone = getZoneByID(pEvt->emitter_id);
            if (!pZone) continue;
           if(pEvt->event_code == ZONE_EVENT_DISABLED)
                pZone->events[4] = pEvt;
           else
                pZone->events[pEvt->event_code] = pEvt;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Zones(void)
{
    if (!tbl_Zone.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Zones()\r\n");

    Ql_MEM_Free(tbl_Zone.t);
    tbl_Zone.t = NULL;
    tbl_Zone.size = 0;

    return RETURN_NO_ERRORS;
}

Zone *getZoneByID(u16 zone_id)
{
    s32 idx = -1;
    BINARY_SEARCH(zone_id,  tbl_Zone.t, tbl_Zone.size, idx);
    return -1 == idx ? NULL : &tbl_Zone.t[idx];
}

Zone_tbl *getZoneTable(s32 *error)
{
    if (error && !tbl_Zone.t) {
        *error = createRuntimeTable_Zones();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Zones() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Zone;
}


bool isArmingDelayActive(Zone *pZone)
{
    if (ZONE_ENTRY_DELAY != pZone->zone_type &&
                ZONE_WALK_THROUGH != pZone->zone_type)
        return FALSE;

    ArmingGroup *pGroup = getArmingGroupByID(pZone->group_id);
    OUT_DEBUG_7("isArmingDelayActive() pGroup=%d \r\n", pGroup);

    if(pGroup){
    OUT_DEBUG_7("isArmingDelayActive() pGroup=%d counter=%d\r\n", pGroup, pGroup->arming_zoneDelay_counter);
    }
    return pGroup && pGroup->arming_zoneDelay_counter;
}

bool isDisarmingDelayActive(Zone *pZone)
{
    if (ZONE_ENTRY_DELAY != pZone->zone_type &&
                ZONE_WALK_THROUGH != pZone->zone_type)
        return FALSE;

    ArmingGroup *pGroup = getArmingGroupByID(pZone->group_id);
    OUT_DEBUG_7("isDisarmingDelayActive() pGroup=%d \r\n", pGroup);

    if(pGroup){
    OUT_DEBUG_7("isDisarmingDelayActive() pGroup=%d counter=%d\r\n", pGroup, pGroup->disarming_zoneDelay_counter);
    }

    return pGroup && pGroup->disarming_zoneDelay_counter;
}

void startArmingDelay(ArmingGroup *group)
{
    group->arming_zoneDelay_counter = 1;
    group->disarming_zoneDelay_counter = 0;
    group->entry_delay_zone = NULL;
}

void startDisarmingDelay(ArmingGroup *group, Zone *entry_delay_zone)
{
    group->arming_zoneDelay_counter = 0;
    group->disarming_zoneDelay_counter = 1;
    group->entry_delay_zone = entry_delay_zone;
}

void stopBothDelays(ArmingGroup *group)
{
    group->arming_zoneDelay_counter = 0;
    group->disarming_zoneDelay_counter = 0;
    group->entry_delay_zone = NULL;
}

Zone *getZoneByParent(u16 puin, DbObjectCode ptype, u16 suin)
{
    Zone *pZone = NULL;
    Zone_tbl *pZoneTable = getZoneTable(NULL);

    for (u16 pos = 0; pos < pZoneTable->size; ++pos) {
        pZone = &pZoneTable->t[pos];
        if (pZone->ptype == ptype
                && (pZone->puin == puin || OBJ_MASTERBOARD == ptype)
                && pZone->suin == suin) {
            break;
        } else {
            pZone = NULL;
        }
    }

    return pZone;
}


//************************************************************************
//* RELAYS
//************************************************************************/
s32 createRuntimeTable_Relays()
{
    if (tbl_Relay.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_Relays()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_RELAYS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Relay.t, &file_size );
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Relay.size = file_size / sizeof(Relay);

    // -- fill parent fields
    for (u16 i = 0; i < tbl_Relay.size; ++i)
    {
        Relay *pRelay = &tbl_Relay.t[i];
        DbObjectCode type = pRelay->ptype;
        u16 uin = pRelay->puin;
        ParentDevice *pParent = getParentDeviceByUIN(uin, type);
        if (pParent) {
            pRelay->parent_device = pParent;
        } else {
            OUT_DEBUG_1("Can't find parent %s #%d for Relay #%d\r\n",
                        getUnitTypeByCode(type), uin, pRelay->suin);
            return ERR_DB_RECORD_NOT_FOUND;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Relays()
{
    if (! tbl_Relay.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Relays()\r\n");

    Ql_MEM_Free(tbl_Relay.t);
    tbl_Relay.t = NULL;
    tbl_Relay.size = 0;

    return RETURN_NO_ERRORS;
}

Relay *getRelayByID(u16 relay_id)
{
    s32 idx = -1;
    BINARY_SEARCH(relay_id, tbl_Relay.t, tbl_Relay.size, idx);
    return -1 == idx ? NULL : &tbl_Relay.t[idx];
}

Relay_tbl *getRelayTable(s32 *error)
{
    if (error && !tbl_Relay.t) {
        *error = createRuntimeTable_Relays();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Relays() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Relay;
}

Relay *getRelayByParent(u16 puin, DbObjectCode ptype, u16 suin)
{
    Relay *pRelay = NULL;
    Relay_tbl *pRelayTable = getRelayTable(NULL);

    for (u16 pos = 0; pos < pRelayTable->size; ++pos) {
        pRelay = &pRelayTable->t[pos];
        if (pRelay->ptype == ptype
                && (pRelay->puin == puin || OBJ_MASTERBOARD == ptype)
                && pRelay->suin == suin) {
            break;
        } else {
            pRelay = NULL;
        }
    }

    return pRelay;
}

s32 setRelayUnitState(Relay *pRelay, PerformerUnitState state, BehaviorPreset *pBehavior)
{
    if (pRelay->state == state &&
            UnitStateMultivibrator != state)
    {
        return RETURN_NO_ERRORS;
    }

    // -- remember the state
    pRelay->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, pRelay->ptype, pRelay->puin, OBJ_RELAY, pRelay->suin)

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pRelay->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.relay.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}



s32 setRelayUnitStateAll(Relay *pRelay, PerformerUnitState state, BehaviorPreset *pBehavior)
{

    // -- remember the state
    pRelay->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, pRelay->ptype, pRelay->puin, OBJ_RELAY, pRelay->suin)

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.relay.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pRelay->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.relay.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}



//************************************************************************
//* LEDS
//************************************************************************/
s32 createRuntimeTable_Leds()
{
    if (tbl_Led.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_Leds()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_LEDS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Led.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS; // FIXME: remove this clause in release code
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Led.size = file_size / sizeof(Led);

    // -- fill parent fields
    for (u16 i = 0; i < tbl_Led.size; ++i)
    {
        Led *pLed = &tbl_Led.t[i];
        DbObjectCode ptype = pLed->ptype;
        u16 puin = pLed->puin;
        ParentDevice *pParent = getParentDeviceByUIN(puin, ptype);
        if (pParent) {
            pLed->parent_device = pParent;
        } else {
            OUT_DEBUG_1("Can't find parent %s #%d for Led #%d\r\n",
                        getUnitTypeByCode(ptype), puin, pLed->suin);
            return ERR_DB_RECORD_NOT_FOUND;
        }

        if (OBJ_ETR == ptype) {
            switch (pLed->suin) {
            case 2:     // ext power led
                pLed->state = UnitStateOn;
                break;
            default:
                pLed->state = UnitStateOff;
                break;
            };
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Leds()
{
    if (! tbl_Led.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Leds()\r\n");

    Ql_MEM_Free(tbl_Led.t);
    tbl_Led.t = NULL;
    tbl_Led.size = 0;

    return RETURN_NO_ERRORS;
}

Led *getLedByID(u16 led_id)
{
    s32 idx = -1;
    BINARY_SEARCH(led_id, tbl_Led.t, tbl_Led.size, idx);
    return -1 == idx ? NULL : &tbl_Led.t[idx];
}

Led_tbl *getLedTable(s32 *error)
{
    if (error && !tbl_Led.t) {
        *error = createRuntimeTable_Leds();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Leds() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Led;
}

Led *getLedByParent(u16 puin, DbObjectCode ptype, u16 suin)
{
    Led *pLed = NULL;
    Led_tbl *pLedTable = getLedTable(NULL);

    for (u16 pos = 0; pos < pLedTable->size; ++pos) {
        pLed = &pLedTable->t[pos];
        if (pLed->ptype == ptype
                && (pLed->puin == puin || OBJ_MASTERBOARD == ptype)
                && pLed->suin == suin) {
            break;
        } else {
            pLed = NULL;
        }
    }

    return pLed;
}

s32 setLedUnitState(Led *pLed, PerformerUnitState state, BehaviorPreset *pBehavior)
{
    if (pLed->state == state &&
            UnitStateMultivibrator != state)
    {
        return RETURN_NO_ERRORS;
    }

    // -- remember the state
    pLed->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, pLed->ptype, pLed->puin, OBJ_LED, pLed->suin)

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.led.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.led.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pLed->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.led.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}


s32 setLedUnitStateALL(Led *pLed, PerformerUnitState state, BehaviorPreset *pBehavior)
{
    // -- remember the state
    pLed->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, pLed->ptype, pLed->puin, OBJ_LED, pLed->suin)

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.led.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.led.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pLed->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.led.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}



//************************************************************************
//* AUDIO
//************************************************************************/

s32 setAudioUnitState(u16 puin, PerformerUnitState state)
{

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    UnitIdent u = {0};
    u.stype = OBJ_Switcher;
    u.suin = 0x01;
    u.type = OBJ_ETR;
    u.uin =  puin;

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];


    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.audio.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.audio.in.CmdTurnOff);
        break;
//    case UnitStateMultivibrator:
//        return ERR_UNDEFINED_BEHAVIOR_PRESET;
//        break;
    }
//    return RETURN_NO_ERRORS;
    return sendToMCU(&u, cmd, data, datalen);
}

//************************************************************************
//* BELLS
//************************************************************************/
s32 createRuntimeTable_Bells()
{
    if (tbl_Bell.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_Bells()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_BELLS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Bell.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS; // FIXME: remove this clause in release code
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Bell.size = file_size / sizeof(Bell);

    // -- fill parent fields
    for (u16 i = 0; i < tbl_Bell.size; ++i)
    {
        Bell *pBell = &tbl_Bell.t[i];
        DbObjectCode type = pBell->ptype;
        u16 uin = pBell->puin;
        ParentDevice *pParent = getParentDeviceByUIN(uin, type);
        if (pParent) {
            pBell->parent_device = pParent;
        } else {
            OUT_DEBUG_1("Can't find parent %s #%d for Bell #%d\r\n",
                        getUnitTypeByCode(type), uin, pBell->suin);
            return ERR_DB_RECORD_NOT_FOUND;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Bells()
{
    if (! tbl_Bell.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Bells()\r\n");

    Ql_MEM_Free(tbl_Bell.t);
    tbl_Bell.t = NULL;
    tbl_Bell.size = 0;

    return RETURN_NO_ERRORS;
}

Bell *getBellByID(u16 bell_id)
{
    s32 idx = -1;
    BINARY_SEARCH(bell_id, tbl_Bell.t, tbl_Bell.size, idx);
    return -1 == idx ? NULL : &tbl_Bell.t[idx];
}

Bell_tbl *getBellTable(s32 *error)
{
    if (error && !tbl_Bell.t) {
        *error = createRuntimeTable_Bells();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Bells() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Bell;
}

Bell *getBellByParent(u16 puin, DbObjectCode ptype, u16 suin)
{
    Bell *pBell = NULL;
    Bell_tbl *pBellTable = getBellTable(NULL);

    for (u16 pos = 0; pos < pBellTable->size; ++pos) {
        pBell = &pBellTable->t[pos];
        if (pBell->ptype == ptype
                && (pBell->puin == puin || OBJ_MASTERBOARD == ptype)
                && pBell->suin == suin) {
            break;
        } else {
            pBell = NULL;
        }
    }

    return pBell;
}

s32 setBellUnitState(Bell *pBell, PerformerUnitState state, BehaviorPreset *pBehavior)
{
    if (pBell->state == state &&
            UnitStateMultivibrator != state)
    {
        return RETURN_NO_ERRORS;
    }

    // -- remember the state
    pBell->state = state;

    // -- send command
    McuCommandCode cmd = CMD_SetValue;
    u8 data[9] = {0};
    u8 datalen = 5;

    CREATE_UNIT_IDENT(u, pBell->ptype, pBell->puin, OBJ_BELL, pBell->suin);

    char *pin = (char *)&data[0];
    *pin = Pin_Input;

    char *lex = (char *)&data[1];

    switch (state) {
    case UnitStateOn:
        Ql_strcpy(lex, lexicon.bell.in.CmdTurnOn);
        break;
    case UnitStateOff:
        Ql_strcpy(lex, lexicon.bell.in.CmdTurnOff);
        break;
    case UnitStateMultivibrator:
        if (pBehavior) {
            pBell->behavior_preset = *pBehavior;
            Ql_strcpy(lex, lexicon.bell.in.CmdMultivibrator);
            datalen += Ar_Lexicon_setMultivibrator_Normal(&data[5],
                                                  pBehavior->pulse_len,
                                                  pBehavior->pause_len,
                                                  pBehavior->pulses_in_batch,
                                                  pBehavior->batch_pause_len);
        }
        else {
            return ERR_UNDEFINED_BEHAVIOR_PRESET;
        }
        break;
    }

    return sendToMCU(&u, cmd, data, datalen);
}


//************************************************************************
//* BUTTONS
//************************************************************************/
s32 createRuntimeTable_Buttons()
{
    if (tbl_Button.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_Buttons()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE(FILENAME_PATTERN_BUTTONS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_Button.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS; // FIXME: remove this clause in release code
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_Button.size = file_size / sizeof(Button);

    // -- fill parent fields
    for (u16 i = 0; i < tbl_Button.size; ++i)
    {
        Button *pButton = &tbl_Button.t[i];
        DbObjectCode type = pButton->ptype;
        u16 uin = pButton->puin;
        ParentDevice *pParent = getParentDeviceByUIN(uin, type);
        if (pParent) {
            pButton->parent_device = pParent;
        } else {
            OUT_DEBUG_1("Can't find parent %s #%d for Button #%d\r\n",
                        getUnitTypeByCode(type), uin, pButton->suin);
            return ERR_DB_RECORD_NOT_FOUND;
        }
    }

    // -- fill events array
    Event_tbl *pEventsTbl = getEventTable(&ret);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("getEventTable() = %d error\r\n", ret);
        return ret;
    }

    Event *pEvt = NULL;
    for (u16 j = 0; j < pEventsTbl->size; ++j) {
        pEvt = &pEventsTbl->t[j];
        if (OBJ_BUTTON == pEvt->emitter_type) {
            Button *pButton = getButtonByID(pEvt->emitter_id);
            if (!pButton) continue;
            pButton->events[pEvt->event_code] = pEvt;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_Buttons()
{
    if (! tbl_Button.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_Buttons()\r\n");

    Ql_MEM_Free(tbl_Button.t);
    tbl_Button.t = NULL;
    tbl_Button.size = 0;

    return RETURN_NO_ERRORS;
}

Button *getButtonByID(u16 button_id)
{
    s32 idx = -1;
    BINARY_SEARCH(button_id, tbl_Button.t, tbl_Button.size, idx);
    return -1 == idx ? NULL : &tbl_Button.t[idx];
}

Button_tbl *getButtonTable(s32 *error)
{
    if (error && !tbl_Button.t) {
        *error = createRuntimeTable_Buttons();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeTable_Buttons() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_Button;
}

Button *getButtonByParent(u16 puin, DbObjectCode ptype, u16 suin)
{
    Button *pButton = NULL;
    Button_tbl *pButtonTable = getButtonTable(NULL);

    for (u16 pos = 0; pos < pButtonTable->size; ++pos) {
        pButton = &pButtonTable->t[pos];
        if (pButton->ptype == ptype
                && (pButton->puin == puin || OBJ_MASTERBOARD == ptype)
                && pButton->suin == suin) {
            break;
        } else {
            pButton = NULL;
        }
    }

    return pButton;
}


//************************************************************************
//* PHONE NUMBERS
//************************************************************************/
s32 createRuntimeArray_Phones(SIMSlot sim)
{
    bool is_sim_1 = (SIM_1 == sim);
    PhoneNumber **phones_array = is_sim_1 ? &arr_PhoneNumbers_SIM1 : &arr_PhoneNumbers_SIM2;

    if (*phones_array)
        return RETURN_NO_ERRORS;

    s32 ret = 0;
    SimSettings *pSimSettings = getSimSettingsArray(&ret);
    if (!pSimSettings)
        return ret;

    OUT_DEBUG_2("createRuntimeArray_Phones(SIM_%d)\r\n", is_sim_1 ? 1 : 2);

    // -- get size of the first list
    char *filename_a = is_sim_1 ?
                        DBFILE_AUTOSYNC(FILENAME_PATTERN_PHONELIST_SIM1) :
                        DBFILE_AUTOSYNC(FILENAME_PATTERN_PHONELIST_SIM2);
    u32 size_a = 0;

    ret = Ql_FS_GetSize(filename_a);
    if (ret < 0) return ret;
    else size_a = ret;

    u8 nPhones_a = size_a / sizeof(PhoneNumber);

    // -- get size of the second list
    char *filename_m = is_sim_1 ?
                        DBFILE(FILENAME_PATTERN_PHONELIST_SIM1) :
                        DBFILE(FILENAME_PATTERN_PHONELIST_SIM2);
    u32 size_m = 0;

    ret = Ql_FS_GetSize(filename_m);
    if (ret < 0) return ret;
    else size_m = ret;

    u8 nPhones_m = size_m / sizeof(PhoneNumber);

    // -- get memory for united list
    u32 list_size = size_a + size_m;

    *phones_array = Ql_MEM_Alloc(list_size);
    if (!*phones_array) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", list_size);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    // -- fill first part of list
    ret = readFromDbFile(filename_a, *phones_array, size_a, 0);
    if (ret < RETURN_NO_ERRORS) {
        Ql_MEM_Free(*phones_array);
        OUT_DEBUG_1("readFromDbFile() = %d error for \"%s\" file.\r\n", ret, filename_a);
        return ret;
    }

    // -- fill second part of list (use the address of the first unfilled item)
    ret = readFromDbFile(filename_m, &(*phones_array)[nPhones_a], size_m, 0);
    if (ret < RETURN_NO_ERRORS) {
        Ql_MEM_Free(*phones_array);
        OUT_DEBUG_1("readFromDbFile() = %d error for \"%s\" file.\r\n", ret, filename_m);
        return ret;
    }

    pSimSettings[sim].phone_list.phones = *phones_array;
    pSimSettings[sim].phone_list.size = nPhones_a + nPhones_m;

    pSimSettings[sim].phone_list._phones_m = &(*phones_array)[nPhones_a];
    pSimSettings[sim].phone_list._size_m = nPhones_m;

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeArray_Phones(SIMSlot sim)
{
    bool is_sim_1 = (SIM_1 == sim);
    PhoneNumber **phones_array = is_sim_1 ? &arr_PhoneNumbers_SIM1 : &arr_PhoneNumbers_SIM2;

    if (!*phones_array)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeArray_Phones(SIM_%d)\r\n", is_sim_1 ? 1 : 2);

    Ql_MEM_Free(*phones_array);
    *phones_array = NULL;

    if (arr_SimCards) {
        arr_SimCards[sim].phone_list.phones = NULL;
        arr_SimCards[sim].phone_list.size = 0;

        arr_SimCards[sim].phone_list._phones_m = NULL;
        arr_SimCards[sim].phone_list._size_m = 0;
    }

    return RETURN_NO_ERRORS;
}

PhoneNumber *getPhonesArray(SIMSlot sim, s32 *error)
{
    PhoneNumber *phones_array = (SIM_1 == sim) ? arr_PhoneNumbers_SIM1 : arr_PhoneNumbers_SIM2;

    if (error && !phones_array) {
        *error = createRuntimeArray_Phones(sim);
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeArray_Phones(SIM_%d) = %d error\r\n", SIM_1 == sim ? 1 : 2, *error);
            return NULL;
        }
    }

    return phones_array;
}


//
s32 createRuntimeArray_AuxPhones(void)
{
    if (arr_PhoneNumbers_Aux)
        return RETURN_NO_ERRORS;

    s32 ret = 0;
    CommonSettings *pCS = getCommonSettings(&ret);
    if (!pCS)
        return ret;

    OUT_DEBUG_2("createRuntimeArray_AuxPhones()\r\n");

    char *filename = DBFILE(FILENAME_PATTERN_PHONELIST_AUX);
    u32 size = 0;

    ret = CREATE_TABLE_FROM_DB_FILE(filename,
                                        (void **)&arr_PhoneNumbers_Aux,
                                        &size);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    pCS->aux_phone_list.phones = arr_PhoneNumbers_Aux;
    pCS->aux_phone_list.size = size / sizeof(PhoneNumber);

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeArray_AuxPhones(void)
{
    if (!arr_PhoneNumbers_Aux)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeArray_AuxPhones()\r\n");

    Ql_MEM_Free(arr_PhoneNumbers_Aux);
    arr_PhoneNumbers_Aux = NULL;

    if (obj_CommonSettings) {
        obj_CommonSettings->aux_phone_list.phones = NULL;
        obj_CommonSettings->aux_phone_list.size = 0;
    }

    return RETURN_NO_ERRORS;
}

PhoneNumber *getAuxPhonesArray(s32 *error)
{
    if (error && !arr_PhoneNumbers_Aux) {
        *error = createRuntimeArray_AuxPhones();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeArray_AuxPhones() = %d error\r\n", *error);
            return NULL;
        }
    }

    return arr_PhoneNumbers_Aux;
}


//************************************************************************
//* IP ADDRESSES
//************************************************************************/
s32 createRuntimeArray_IPs(SIMSlot sim)
{
    bool is_sim_1 = (SIM_1 == sim);
    IpAddress **ip_array = is_sim_1 ? &arr_IpAddresses_SIM1 : &arr_IpAddresses_SIM2;

    if (*ip_array)
        return RETURN_NO_ERRORS;

    s32 ret = 0;
    SimSettings *pSimSettings = getSimSettingsArray(&ret);
    if (!pSimSettings)
        return ret;

    OUT_DEBUG_2("createRuntimeArray_IPs(SIM_%d)\r\n", is_sim_1 ? 1 : 2);

    // -- get size of the first list
    char *filename_a = is_sim_1 ?
                                DBFILE_AUTOSYNC(FILENAME_PATTERN_IPLIST_SIM1) :
                                DBFILE_AUTOSYNC(FILENAME_PATTERN_IPLIST_SIM2);
    u32 size_a = 0;

    ret = Ql_FS_GetSize(filename_a);
    if (ret < 0) return ret;
    else size_a = ret;

    u8 nAddresses_a = size_a / sizeof(IpAddress);

    // -- get size of the second list
    char *filename_m = is_sim_1 ?
                        DBFILE(FILENAME_PATTERN_IPLIST_SIM1) :
                        DBFILE(FILENAME_PATTERN_IPLIST_SIM2);
    u32 size_m = 0;

    ret = Ql_FS_GetSize(filename_m);
    if (ret < 0) return ret;
    else size_m = ret;

    u8 nAddresses_m = size_m / sizeof(IpAddress);

    // -- get memory for united list
    u32 list_size = size_a + size_m;

    *ip_array = Ql_MEM_Alloc(list_size);
    if (!*ip_array) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", list_size);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    // -- fill first part of list
    ret = readFromDbFile(filename_a, *ip_array, size_a, 0);
    if (ret < RETURN_NO_ERRORS) {
        Ql_MEM_Free(*ip_array);
        OUT_DEBUG_1("readFromDbFile() = %d error for \"%s\" file.\r\n", ret, filename_a);
        return ret;
    }

    // -- fill second part of list (use the address of the first unfilled item)
    ret = readFromDbFile(filename_m, &(*ip_array)[nAddresses_a], size_m, 0);
    if (ret < RETURN_NO_ERRORS) {
        Ql_MEM_Free(*ip_array);
        OUT_DEBUG_1("readFromDbFile() = %d error for \"%s\" file.\r\n", ret, filename_m);
        return ret;
    }

    pSimSettings[sim].udp_ip_list.IPs = *ip_array;
    pSimSettings[sim].udp_ip_list.size = nAddresses_a + nAddresses_m;

    pSimSettings[sim].udp_ip_list._IPs_m = &(*ip_array)[nAddresses_a];
    pSimSettings[sim].udp_ip_list._size_m = nAddresses_m;

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeArray_IPs(SIMSlot sim)
{
    bool is_sim_1 = (SIM_1 == sim);
    IpAddress **ip_array = is_sim_1 ? &arr_IpAddresses_SIM1 : &arr_IpAddresses_SIM2;

    if (!*ip_array)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeArray_IPs(SIM_%d)\r\n", is_sim_1 ? 1 : 2);

    Ql_MEM_Free(*ip_array);
    *ip_array = NULL;

    if (arr_SimCards) {
        arr_SimCards[sim].udp_ip_list.IPs = NULL;
        arr_SimCards[sim].udp_ip_list.size = 0;

        arr_SimCards[sim].udp_ip_list._IPs_m = NULL;
        arr_SimCards[sim].udp_ip_list._size_m = 0;
    }

    return RETURN_NO_ERRORS;
}

IpAddress *getIPsArray(SIMSlot sim, s32 *error)
{
    IpAddress *ip_array = (SIM_1 == sim) ? arr_IpAddresses_SIM1 : arr_IpAddresses_SIM2;

    if (error && !ip_array) {
        *error = createRuntimeArray_IPs(sim);
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeArray_IPs(SIM_%d) = %d error\r\n", SIM_1 == sim ? 1 : 2, *error);
            return NULL;
        }
    }

    return ip_array;
}

bool isIPAddressValid(IpAddress *addr)
{
    for (u8 i = 0; i < 4; ++i) {
        if (addr->address[i])
            return TRUE;
    }
    return FALSE;
}


//************************************************************************
//* SIM CARDS
//************************************************************************/
s32 createRuntimeArray_SimSettings(void)
{
    if (arr_SimCards)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeArray_SimSettings()\r\n");

    char *filename = DBFILE(FILENAME_PATTERN_SIMCARDS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE(filename,
                                        (void **)&arr_SimCards,
                                        NULL);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeArray_SimSettings(void)
{
    if (!arr_SimCards)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeArray_SimSettings()\r\n");

    Ql_MEM_Free(arr_SimCards);
    arr_SimCards = NULL;

    return RETURN_NO_ERRORS;
}

SimSettings *getSimSettingsArray(s32 *error)
{
    if (error && !arr_SimCards) {
        *error = createRuntimeArray_SimSettings();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeArray_SimSettings() = %d error\r\n", *error);
            return NULL;
        }
    }

    return arr_SimCards;
}

char *getPhoneByIndex(SIMSlot sim, u8 phone_index)
{
    return ((phone_index < arr_SimCards[sim].phone_list.size)
              ? arr_SimCards[sim].phone_list.phones[phone_index].number
              : NULL);
}

s16 findPhoneIndex(const char *phone, SIMSlot *simcard)
{
    for (SIMSlot sim = SIM_1; sim <= SIM_2; ++sim) {
        PhoneList *pPhones = &getSimSettingsArray(0)[sim].phone_list;

        for (u8 idx = 0; idx < pPhones->size; ++idx) {
            if (Ql_strstr(pPhones->phones[idx].number, phone)) {
                *simcard = sim;
                return idx;
            }
        }
    }

    return -1;
}

u8 *getIpAddressByIndex(SIMSlot sim, u8 IpAddress_index)
{
    return ((IpAddress_index < arr_SimCards[sim].udp_ip_list.size)
              ? arr_SimCards[sim].udp_ip_list.IPs[IpAddress_index].address
              : NULL);
}

s16 findIpAddressIndex(const u8 *ip, SIMSlot *simcard)
{
    for (SIMSlot sim = SIM_1; sim <= SIM_2; ++sim) {
        IpList *pIPs = &getSimSettingsArray(0)[sim].udp_ip_list;

        for (u8 idx = 0; idx < pIPs->size; ++idx) {
            if (pIPs->IPs[idx].address[0] == ip[0] &&
                    pIPs->IPs[idx].address[1] == ip[1] &&
                    pIPs->IPs[idx].address[2] == ip[2] &&
                    pIPs->IPs[idx].address[3] == ip[3])
            {
                *simcard = sim;
                return idx;
            }
        }
    }

    return -1;
}


//************************************************************************
//* PULT_MESSAGE BUILDER
//************************************************************************/
s32 createRuntimeObject_PultMessageBuilder(void)
{
    if (obj_MessageBuilder)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeObject_PultMessageBuilder()\r\n");

    s32 ret = 0;
    CommonSettings *pCS = getCommonSettings(&ret);
    if (!pCS)
        return ret;

    obj_MessageBuilder = Ql_MEM_Alloc(sizeof(PultMessageBuilder));
    if (!obj_MessageBuilder)
        return ERR_GET_NEW_MEMORY_FAILED;

    // -- register the codec's mapping tables and functions
    init_pultMessageBuilder(obj_MessageBuilder, pCS->codec_type);

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeObject_PultMessageBuilder()
{
    if (!obj_MessageBuilder)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeObject_PultMessageBuilder()\r\n");

    Ql_MEM_Free(obj_MessageBuilder);
    obj_MessageBuilder = NULL;

    return RETURN_NO_ERRORS;
}

PultMessageBuilder *getMessageBuilder(s32 *error)
{
    if (error && !obj_MessageBuilder) {
        *error = createRuntimeObject_PultMessageBuilder();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeObject_PultMessageBuilder() = %d error\r\n", *error);
            return NULL;
        }
    }

    return obj_MessageBuilder;
}


//************************************************************************
//* COMMON SETTINGS
//************************************************************************/
s32 createRuntimeObject_CommonSettings(void)
{
    if (obj_CommonSettings)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeObject_CommonSettings()\r\n");

    char *filename = DBFILE(FILENAME_PATTERN_COMMON_SETTINGS);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename,
                                         (void **)&obj_CommonSettings,
                                         NULL );
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    // -- find the masterboard
    ParentDevice_tbl *parents_table = getParentDevicesTable(&ret);
    if (ret < RETURN_NO_ERRORS)
        return ret;

    // use first systemboard in DB
    obj_CommonSettings->p_masterboard = NULL;
    for (u16 pos = 0; pos < parents_table->size; ++pos) {
        if (OBJ_MASTERBOARD == parents_table->t[pos].type) {
            obj_CommonSettings->p_masterboard = &parents_table->t[pos];
            break;
        }
    }
    if (!obj_CommonSettings->p_masterboard) {
        OUT_DEBUG_1("Masterboard did not found in settings DB\r\n");
    }

    // -- fill the boolean fields
    obj_CommonSettings->bMainBellOk = TRUE;
    obj_CommonSettings->bSystemTrouble = FALSE;

    // -- fill DTMF signal parameters
    if (0 == obj_CommonSettings->dtmf_pulse_ms)
        obj_CommonSettings->dtmf_pulse_ms = DTMF_PULSE_DEFAULT_LEN;
    if (0 == obj_CommonSettings->dtmf_pause_ms)
        obj_CommonSettings->dtmf_pause_ms = DTMF_PULSE_DEFAULT_LEN;

    // -- fill ADC quotients
    if (0 == obj_CommonSettings->adc_a) {
        obj_CommonSettings->adc_a = ADC_DEFAULT_A;
        obj_CommonSettings->adc_b = ADC_DEFAULT_B;
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeObject_CommonSettings(void)
{
    if (!obj_CommonSettings)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeObject_CommonSettings()\r\n");

    Ql_MEM_Free(obj_CommonSettings);
    obj_CommonSettings = NULL;

    return RETURN_NO_ERRORS;
}

CommonSettings *getCommonSettings(s32 *error)
{
    if (error && !obj_CommonSettings) {
        *error = createRuntimeObject_CommonSettings();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeObject_CommonSettings() = %d error\r\n", *error);
            return NULL;
        }
    }

    return obj_CommonSettings;
}

u8 debugLevel(void)
{
    if (! obj_CommonSettings) {
        return MAX_DEBUG_LEVEL;
    }

    return obj_CommonSettings->debug_level;
}

char *getAuxPhoneByIndex(u8 aux_phone_index)
{
    return ((aux_phone_index < obj_CommonSettings->aux_phone_list.size)
              ? obj_CommonSettings->aux_phone_list.phones[aux_phone_index].number
              : NULL);
}

s32 updateMasterboardUin(u16 uin)
{
    if (!obj_CommonSettings)
        return ERR_DB_OBJECT_NOT_INITIALIZED;

    obj_CommonSettings->p_masterboard->uin = uin;

    Zone_tbl *pZones = getZoneTable(0);
    for (u16 i = 0; i < pZones->size; ++i) {
        if(OBJ_MASTERBOARD == pZones->t[i].ptype)
            pZones->t[i].puin = uin;
    }

    Relay_tbl *pRelays = getRelayTable(0);
    for (u16 i = 0; i < pRelays->size; ++i) {
        if(OBJ_MASTERBOARD == pRelays->t[i].ptype)
            pRelays->t[i].puin = uin;
    }

    Bell_tbl *pBells = getBellTable(0);
    for (u16 i = 0; i < pBells->size; ++i) {
        if(OBJ_MASTERBOARD == pBells->t[i].ptype)
            pBells->t[i].puin = uin;
    }

    Led_tbl *pLeds = getLedTable(0);
    for (u16 i = 0; i < pLeds->size; ++i) {
        if(OBJ_MASTERBOARD == pLeds->t[i].ptype)
            pLeds->t[i].puin = uin;
    }

    Button_tbl *pButtons = getButtonTable(0);
    for (u16 i = 0; i < pButtons->size; ++i) {
        if(OBJ_MASTERBOARD == pButtons->t[i].ptype)
            pButtons->t[i].puin = uin;
    }

    return RETURN_NO_ERRORS;
}


//************************************************************************
//* SYSTEM INFO
//************************************************************************/
s32 createRuntimeObject_SystemInfo(void)
{
    if (obj_SystemInfo)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeObject_SystemInfo()\r\n");

    char *filename = DBFILE(FILENAME_PATTERN_SYSTEMINFO);
    s32 ret = CREATE_TABLE_FROM_DB_FILE(filename, (void **)&obj_SystemInfo, NULL);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeObject_SystemInfo(void)
{
    if (!obj_SystemInfo)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeObject_SystemInfo()\r\n");

    Ql_MEM_Free(obj_SystemInfo);
    obj_SystemInfo = NULL;

    return RETURN_NO_ERRORS;
}

s32 updateRuntimeObject_SystemInfo(void)
{
    s32 ret = destroyRuntimeObject_SystemInfo();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("destroyRuntimeObject_SystemInfo() = %d error\r\n", ret);
        return ret;
    }

    ret = createRuntimeObject_SystemInfo();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("createRuntimeObject_SystemInfo() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

SystemInfo *getSystemInfo(s32 *error)
{
    if (error && !obj_SystemInfo) {
        *error = createRuntimeObject_SystemInfo();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeObject_SystemInfo() = %d error\r\n", *error);
            return NULL;
        }
    }

    return obj_SystemInfo;
}

bool checkDBStructureVersion(void)
{
    OUT_DEBUG_2("checkDBStructureVersion()\r\n");

    if (! obj_SystemInfo) {
        s32 ret = createRuntimeObject_SystemInfo();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeObject_SystemInfo() = %d error\r\n", ret);
            return FALSE;
        }
    }

    bool success = Ar_Lexicon_isEqual(obj_SystemInfo->db_structure_version, DB_STRUCTURE_VERSION);
    if (success) {
        OUT_DEBUG_3("Compatible DB version found: %s\r\n", obj_SystemInfo->db_structure_version);
    } else {
        OUT_DEBUG_3("Incompatible DB version found: %s\r\n", obj_SystemInfo->db_structure_version);
    }

    return success;
}

//************************************************************************
//* Group State File
//************************************************************************/
s32 createRuntimeTable_GroupStateFile()
{
    if (tbl_GroupStateFileList.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("createRuntimeTable_GroupStateFile()\r\n");

    u32 file_size = 0;
    char *filename = DBFILE_CONFIG(FILENAME_PATTERN_CONFIG_DB_FILE);
    s32 ret = CREATE_TABLE_FROM_DB_FILE( filename, (void **)&tbl_GroupStateFileList.t, &file_size );
    if (ERR_DB_FILE_IS_EMPTY == ret) {
        ret = RETURN_NO_ERRORS;
    } else if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("CREATE_TABLE_FROM_DB_FILE( \"%s\" ) = %d error.\r\n", filename, ret);
        return ret;
    }

    tbl_GroupStateFileList.size = file_size / sizeof(GroupStateFile);
    return RETURN_NO_ERRORS;
}

s32 destroyRuntimeTable_GroupStateFile()
{
    if (! tbl_GroupStateFileList.t)
        return RETURN_NO_ERRORS;

    OUT_DEBUG_2("destroyRuntimeTable_GroupStateFile()\r\n");

    Ql_MEM_Free(tbl_GroupStateFileList.t);
    tbl_GroupStateFileList.t = NULL;
    tbl_GroupStateFileList.size = 0;

    return RETURN_NO_ERRORS;
}

GroupStateFileList *getGroupStateFile(s32 *error)
{
    if (error && !tbl_GroupStateFileList.t) {
        *error = createRuntimeTable_GroupStateFile();
        if (*error < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("createRuntimeArray_GroupStateFile() = %d error\r\n", *error);
            return NULL;
        }
    }

    return &tbl_GroupStateFileList;
}
