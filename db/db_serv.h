/*
 * db_access.h
 *
 * Created: 15.10.2012 14:52:36
 *  Author: user
 */
//master

//master 2

//test_branch

#ifndef DB_SERV_H
#define DB_SERV_H

#include "ql_type.h"
#include "configurator/configurator.h"
#include "common/configurator_protocol.h"


/* -- DB DIRECTORY -- */
#define DB_ROOT                     "DB"
// directories used for manual configuration
#define DB_DIR_WORKING              DB_ROOT"\\WORKING"
#define DB_DIR_TEMP                 DB_ROOT"\\TEMP"
#define DB_DIR_BACKUP               DB_ROOT"\\BACKUP"
#define DB_DIR_FACTORY              DB_ROOT"\\FACTORY"
// directories used for auto configuration
#define DB_DIR_AUTOSYNC             DB_ROOT"\\AUTOSYNC"
#define DB_DIR_AUTOSYNC_TEMP        DB_ROOT"\\AUTOSYNC_TEMP"
// dynamic fields
#define DB_DIR_DYNAMIC              DB_ROOT"\\DYNAMIC"
// directories used for FOTA upgrade
#define DB_DIR_FOTA                 "FOTA"
#define DB_DIR_CONFIG               "CONFIG"
//
/* -- DB FULL FILEPATH -- */
#define DBFILE(name)                DB_DIR_WORKING"\\"name
#define DBFILE_TEMP(name)           DB_DIR_TEMP"\\"name
#define DBFILE_AUTOSYNC(name)       DB_DIR_AUTOSYNC"\\"name
#define DBFILE_AUTOSYNC_TEMP(name)  DB_DIR_AUTOSYNC_TEMP"\\"name
#define DBFILE_DYNAMIC(name)        DB_DIR_DYNAMIC"\\"name
#define DBFILE_FOTA(name)           DB_DIR_FOTA"\\"name

/* -- DB FILENAME -- */
// -- dynamic
#define FILENAME_PATTERN_DYNAMIC_FIELDS           "dynamic_fields"

// -- working
#define FILENAME_PATTERN_SQLITE_DB_FILE           "sqlite_db_file"
#define FILENAME_PATTERN_TOUCHMEMORY_CODES        "touchmemory_codes"
#define FILENAME_PATTERN_KEYBOARD_CODES           "keyboard_codes"
#define FILENAME_PATTERN_ARMING_GROUPS			  "arming_groups"
#define FILENAME_PATTERN_ZONES					  "zones"
#define FILENAME_PATTERN_RELAYS					  "relays"
#define FILENAME_PATTERN_LEDS					  "leds"
#define FILENAME_PATTERN_BELLS					  "bells"
#define FILENAME_PATTERN_BUTTONS    			  "buttons"
#define FILENAME_PATTERN_PARENT_DEVS              "parent_devices"
#define FILENAME_PATTERN_COMMON_SETTINGS          "common_settings"
#define FILENAME_PATTERN_SIMCARDS                 "simcards"
#define FILENAME_PATTERN_EVENTS                   "events"
#define FILENAME_PATTERN_BEHAVIOR_PRESETS         "behavior_presets"
#define FILENAME_PATTERN_REACTIONS                "reactions"
#define FILENAME_PATTERN_PHONELIST_AUX            "phonelist_aux"
#define FILENAME_PATTERN_RELATIONS_ETR_AGROUP     "relations_etr_agroup"

// -- autosync
#define FILENAME_PATTERN_COMMON_AUTOSYNC_SETTINGS "common_settings_autosync"

// -- common for working and autosync
#define FILENAME_PATTERN_SYSTEMINFO               "system_info"
#define FILENAME_PATTERN_IPLIST_SIM1              "iplist_sim1"
#define FILENAME_PATTERN_IPLIST_SIM2              "iplist_sim2"
#define FILENAME_PATTERN_PHONELIST_SIM1           "phonelist_sim1"
#define FILENAME_PATTERN_PHONELIST_SIM2           "phonelist_sim2"

// -- FOTA
#define FILENAME_PATTERN_FOTA_DB_FILE              "fota_file"
#define FOTA_READ_SIZE                              512
/* -- FUNCTIONS -- */
s32 prepareDBinRAM(void);
s32 reloadDBinRAM(void);
s32 process_received_settings(RxConfiguratorPkt *pData);
void rollbackConfiguring(void);

s32  saveSettingsToAutosyncTemporaryDB(RxConfiguratorPkt *pPkt);

s32 actualizationSIMCardLists(DbObjectCode DataType);

void DBRestoreLedMessage(void);

s32  FOTA_Upgrade(void);

typedef struct {
    u16 files_quantity;
    s32 all_files_len;

    u16 size_file[30];
    u16 parts_in_file[30];
    u16 pkt_struct_size[30];
    u16 db_struct_size[30];
    // current
    u16 current_file_index;
    u32 current_file_offset;
    u16 current_file_part;
    u32 mustread;
    bool Start_Cycl;

} OneFileTransferParam;

extern OneFileTransferParam CurrParam;

#define _RESET_ONE_FILE_TRANSFER_PARAM   \
    CurrParam.all_files_len = 0;   \
    CurrParam.current_file_index = 0;  \
    CurrParam.current_file_offset = 0; \
    CurrParam.files_quantity = 0;  \
    CurrParam.current_file_part = 0;   \
    CurrParam.Start_Cycl = FALSE;   \
    CurrParam.mustread = 0;

#endif /* DB_SERV_H */
