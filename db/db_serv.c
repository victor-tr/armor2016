/*
 * db_access.c
 *
 * Created: 15.10.2012 14:52:55
 *  Author: user
 */
//
#include "db_serv.h"
#include "db/db_macro.h"
#include "db/fs.h"
#include "db/db.h"

#include "ql_memory.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "service/crc.h"
#include "service/humanize.h"

#include "core/system.h"
#include "core/power.h"

#include "configurator/configurator.h"
#include "configurator/configurator_tx_buffer.h"

#include "states/armingstates.h"

#include "ql_fota.h"

// -- prototypes
//static inline void unpackBytesToFlags(u8 *in_bytesArray, u16 hByteIdx, u16 lByteIdx, bool out_bitsArray[]);
//static inline void packFlagsToBytes(void);


static bool createTempDBSkeleton_flash(void);
static bool restoreBackupDB_flash(void);
static s32  _clearWorkingDB_ram(void);
static s32  _createWorkingDB_ram(void);
static s32  saveSettingsToTemporaryDB(RxConfiguratorPkt *pPkt);

static s32  processDownloadingSettingsToPC(bool reset);


// -- DB paths
static char * const db_dir_working  = DB_DIR_WORKING;
static char * const db_dir_temp     = DB_DIR_TEMP;
static char * const db_dir_backup   = DB_DIR_BACKUP;
static char * const db_dir_factory  = DB_DIR_FACTORY;
static char * const db_dir_autosync      = DB_DIR_AUTOSYNC;
static char * const db_dir_autosync_temp = DB_DIR_AUTOSYNC_TEMP;
static char * const db_dir_dynamic  = DB_DIR_DYNAMIC;
static char * const db_dir_fota  = DB_DIR_FOTA;


// -- temp DB file list
static char * const db_temp_filelist[] = {
    DBFILE_TEMP( FILENAME_PATTERN_SQLITE_DB_FILE ),
    DBFILE_TEMP( FILENAME_PATTERN_ARMING_GROUPS ),
    DBFILE_TEMP( FILENAME_PATTERN_TOUCHMEMORY_CODES ),
    DBFILE_TEMP( FILENAME_PATTERN_KEYBOARD_CODES ),
    DBFILE_TEMP( FILENAME_PATTERN_ZONES ),
    DBFILE_TEMP( FILENAME_PATTERN_RELAYS ),
    DBFILE_TEMP( FILENAME_PATTERN_LEDS ),
    DBFILE_TEMP( FILENAME_PATTERN_BELLS ),
    DBFILE_TEMP( FILENAME_PATTERN_BUTTONS ),
    DBFILE_TEMP( FILENAME_PATTERN_PARENT_DEVS ),
    DBFILE_TEMP( FILENAME_PATTERN_COMMON_SETTINGS ),
    DBFILE_TEMP( FILENAME_PATTERN_SYSTEMINFO ),
    DBFILE_TEMP( FILENAME_PATTERN_IPLIST_SIM1 ),
    DBFILE_TEMP( FILENAME_PATTERN_IPLIST_SIM2 ),
    DBFILE_TEMP( FILENAME_PATTERN_PHONELIST_SIM1 ),
    DBFILE_TEMP( FILENAME_PATTERN_PHONELIST_SIM2 ),
    DBFILE_TEMP( FILENAME_PATTERN_PHONELIST_AUX ),
    DBFILE_TEMP( FILENAME_PATTERN_SIMCARDS ),
    DBFILE_TEMP( FILENAME_PATTERN_EVENTS ),
    DBFILE_TEMP( FILENAME_PATTERN_BEHAVIOR_PRESETS ),
    DBFILE_TEMP( FILENAME_PATTERN_REACTIONS ),
    DBFILE_TEMP( FILENAME_PATTERN_RELATIONS_ETR_AGROUP )
};

static char * const db_autosync_temp_filelist[] = {
    DBFILE_AUTOSYNC_TEMP( FILENAME_PATTERN_SYSTEMINFO ),
    DBFILE_AUTOSYNC_TEMP( FILENAME_PATTERN_IPLIST_SIM1 ),
    DBFILE_AUTOSYNC_TEMP( FILENAME_PATTERN_IPLIST_SIM2 ),
    DBFILE_AUTOSYNC_TEMP( FILENAME_PATTERN_PHONELIST_SIM1 ),
    DBFILE_AUTOSYNC_TEMP( FILENAME_PATTERN_PHONELIST_SIM2 ),
    DBFILE_AUTOSYNC_TEMP( FILENAME_PATTERN_COMMON_AUTOSYNC_SETTINGS )
};


static char * const db_autosync_filelist[] = {
    DBFILE_AUTOSYNC( FILENAME_PATTERN_SYSTEMINFO ),
    DBFILE_AUTOSYNC( FILENAME_PATTERN_IPLIST_SIM1 ),
    DBFILE_AUTOSYNC( FILENAME_PATTERN_IPLIST_SIM2 ),
    DBFILE_AUTOSYNC( FILENAME_PATTERN_PHONELIST_SIM1 ),
    DBFILE_AUTOSYNC( FILENAME_PATTERN_PHONELIST_SIM2 ),
    DBFILE_AUTOSYNC( FILENAME_PATTERN_COMMON_AUTOSYNC_SETTINGS )
};


//// --  DB file list
static char * const db_filelist[] = {
    DBFILE( FILENAME_PATTERN_PARENT_DEVS ),
    DBFILE( FILENAME_PATTERN_PARENT_DEVS ),
    DBFILE( FILENAME_PATTERN_PARENT_DEVS ),
    DBFILE( FILENAME_PATTERN_PARENT_DEVS ),
    DBFILE( FILENAME_PATTERN_COMMON_SETTINGS ),
    DBFILE( FILENAME_PATTERN_ARMING_GROUPS ),
    DBFILE( FILENAME_PATTERN_TOUCHMEMORY_CODES ),
    DBFILE( FILENAME_PATTERN_KEYBOARD_CODES ),
    DBFILE( FILENAME_PATTERN_ZONES ),
    DBFILE( FILENAME_PATTERN_RELAYS ),
    DBFILE( FILENAME_PATTERN_LEDS ),
    DBFILE( FILENAME_PATTERN_BELLS ),
    DBFILE( FILENAME_PATTERN_BUTTONS ),
    DBFILE( FILENAME_PATTERN_SYSTEMINFO ),
    DBFILE( FILENAME_PATTERN_IPLIST_SIM1 ),
    DBFILE( FILENAME_PATTERN_IPLIST_SIM2 ),
    DBFILE( FILENAME_PATTERN_PHONELIST_SIM1 ),
    DBFILE( FILENAME_PATTERN_PHONELIST_SIM2 ),
    DBFILE( FILENAME_PATTERN_PHONELIST_AUX ),
    DBFILE( FILENAME_PATTERN_SIMCARDS ),
    DBFILE( FILENAME_PATTERN_EVENTS ),
    DBFILE( FILENAME_PATTERN_BEHAVIOR_PRESETS ),
    DBFILE( FILENAME_PATTERN_REACTIONS ),
    DBFILE( FILENAME_PATTERN_RELATIONS_ETR_AGROUP )

};


// --  DB code list
static u8 const db_codelist[] = {
    OBJ_MASTERBOARD,
    OBJ_ETR,
    OBJ_RZE,
    OBJ_RAE,
    OBJ_COMMON_SETTINGS ,
    OBJ_ARMING_GROUP ,
    OBJ_TOUCHMEMORY_CODE ,
    OBJ_KEYBOARD_CODE ,
    OBJ_ZONE ,
    OBJ_RELAY ,
    OBJ_LED ,
    OBJ_BELL ,
    OBJ_BUTTON ,
    OBJ_SYSTEM_INFO ,
    OBJ_IPLIST_SIM1 ,
    OBJ_IPLIST_SIM2 ,
    OBJ_PHONELIST_SIM1 ,
    OBJ_PHONELIST_SIM2 ,
    OBJ_PHONELIST_AUX ,
    OBJ_SIM_CARD ,
    OBJ_EVENT ,
    OBJ_BEHAVIOR_PRESET ,
    OBJ_REACTION ,
    OBJ_RELATION_ETR_AGROUP

};


// --   pkt_max list
static u8 const pkt_max_list[] = {
    MBPKT_MAX,
    ETRPKT_MAX,
    EPKT_MAX,
    EPKT_MAX,
    CSPKT_MAX ,
    AGPKT_MAX ,
    TMCPKT_MAX ,
    KBDCPKT_MAX ,
    ZPKT_MAX ,
    RPKT_MAX ,
    LEDPKT_MAX ,
    BELLPKT_MAX ,
    BTNPKT_MAX ,
    SINFPKT_MAX ,
    IPAPKT_MAX ,
    IPAPKT_MAX ,
    PHNPKT_MAX ,
    PHNPKT_MAX ,
    PHNPKT_MAX ,
    SIMPKT_MAX ,
    EVTPKT_MAX ,
    BHPPKT_MAX ,
    RCNPKT_MAX ,
    REAGPKT_MAX

};


// --   object list
static u8 const obj_list[] = {
    sizeof(ParentDevice) ,
    sizeof(ParentDevice) ,
    sizeof(ParentDevice) ,
    sizeof(ParentDevice) ,
    sizeof(CommonSettings) ,
    sizeof(ArmingGroup) ,
    sizeof(TouchmemoryCode) ,
    sizeof(KeyboardCode) ,
    sizeof(Zone) ,
    sizeof(Relay) ,
    sizeof(Led) ,
    sizeof(Bell) ,
    sizeof(Button) ,
    sizeof(SystemInfo) ,
    sizeof(IpAddress) ,
    sizeof(IpAddress) ,
    sizeof(PhoneNumber) ,
    sizeof(PhoneNumber) ,
    sizeof(PhoneNumber) ,
    sizeof(SimSettings) ,
    sizeof(Event) ,
    sizeof(BehaviorPreset) ,
    sizeof(Reaction) ,
    sizeof(RelationETRArmingGroup)

};


static bool _use_factory_settings = FALSE;
static bool _bTempDbSaved = FALSE;
static bool _bFotaRunOne = FALSE;

OneFileTransferParam CurrParam;

//
s32 process_received_settings(RxConfiguratorPkt *pData)
{
    /* file code <=> DB table name <=> data_type */
    DbObjectCode file_code = pData->dataType;

    // -- check the KEY
    if (pData->sessionKey != Ar_Configurator_transactionKey())
        return ERR_BAD_PKT_SENDER;

    // -- if previous pkt succesfully delivered => now received new pkt => must clear previous sendBuffer
    if (pData->pktIdx == Ar_Configurator_lastRecvPktId() + 1 + pData->repeatCounter)
        clearTxBuffer_configurator();      // if normal pkt received => clear previous send buffer

    // -- check PKT INDEX
    if (pData->pktIdx != Ar_Configurator_lastRecvPktId() + 1 + pData->repeatCounter) {
        // handle repeated packet
        if (pData->pktIdx == Ar_Configurator_lastRecvPktId() + pData->repeatCounter) {
            OUT_DEBUG_3("Repeated packet detected. Do nothing.\r\n");
            Ar_Configurator_stopWaitingResponse();
            sendBufferedPacket_configurator();
            return RETURN_NO_ERRORS;
        } else {
            return ERR_BAD_PKT_INDEX;
        }
    }

    Ar_Configurator_stopWaitingResponse();
    Ar_Configurator_setLastRecvPktId(pData->pktIdx - pData->repeatCounter);

    switch (file_code) {
    case PC_CONFIGURATION_SAVE_AS_WORKING:
    {
        s32 ret =  0;

        // -- check DB version
        SystemInfo *pSysInfo = NULL;
        ret = CREATE_SINGLE_OBJECT_FROM_DB_FILE(DBFILE_TEMP(FILENAME_PATTERN_SYSTEMINFO),
                                                    (void **)&pSysInfo, sizeof(SystemInfo), 0);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("Temp DB saving: CREATE_SINGLE_OBJECT_FROM_DB_FILE() = %d error\r\n", ret);
            return ERR_DB_SAVING_FAILED;
        }

        bool success = (0 == Ql_strcmp(pSysInfo->db_structure_version,
                                       DB_STRUCTURE_VERSION));
        if (success) {
            OUT_DEBUG_3("Compatible DB version will be saved: %s\r\n",
                        pSysInfo->db_structure_version);
        }
        else {
            OUT_DEBUG_1("DB version %s is incompatible with the device firmware version. Saving FAILED.\r\n",
                        pSysInfo->db_structure_version);
            rollbackConfiguring();
            return ERR_DB_SAVING_FAILED;
        }

        // -- save configurator settings
        if (_use_factory_settings)
            success = copyDir(db_dir_temp, db_dir_factory, NULL, TRUE);
        else
            success = copyDir(db_dir_temp, db_dir_working, db_dir_backup, TRUE);

        if (!success) {
            OUT_DEBUG_1("copyDir(src=\"%s\", dst=\"%s\") = FALSE\r\n"
                        , db_dir_temp, db_dir_working);
            rollbackConfiguring();
            return ERR_DB_SAVING_FAILED;
        }

        // -- save autosync settings
//        success = copyDir(db_dir_autosync_temp, db_dir_autosync, NULL, TRUE);
//        if (!success) {
//            OUT_DEBUG_1("copyDir(src=\"%s\", dst=\"%s\") = FALSE\r\n"
//                        , db_dir_autosync_temp, db_dir_autosync);
//            restoreBackupDB_flash();
//            rollbackConfiguring();
//            return ERR_DB_SAVING_FAILED;
//        }

        // create autosync dir skeleton
        //++++++++++++++++++++++
        if (! createDirRecursively(db_dir_autosync)) {
            OUT_DEBUG_1("createDir(\"%s\") = FALSE\r\n", db_dir_autosync);
        }
        //++++++++++++++++++++++

        for (u8 i = 0; i < sizeof(db_autosync_filelist)/sizeof(u8*); ++i) {

            s32 fh = Ql_FS_Open(db_autosync_filelist[i], QL_FS_CREATE_ALWAYS);
            if (fh < QL_RET_OK) {
                OUT_DEBUG_1("Error %d occured while creating or opening file \"%s\".\r\n",
                            fh, db_autosync_filelist[i]);
                return ERR_DB_SAVING_FAILED;
            }
            s32 ret = Ql_FS_Truncate(fh);
            if (ret < QL_RET_OK) {
                OUT_DEBUG_1("Error %d occured while truncating file \"%s\".\r\n",
                            ret, db_autosync_filelist[i]);
                return ERR_DB_SAVING_FAILED;
            }
            Ql_FS_Close(fh);
            OUT_DEBUG_3("File \"%s\" created.\r\n", db_autosync_filelist[i]);
        }

        Ar_Configurator_sendACK(file_code);
        _bTempDbSaved = TRUE;
        break;
    }

    case PC_RESTORE_FACTORY_SETTINGS:
    {
        // TODO: implement me
        Ar_Configurator_sendACK(file_code);
        break;
    }

    case PC_CONFIGURATION_END:
    {
        // clear garbage
        if (! deleteDir(db_dir_temp)) {
            OUT_DEBUG_1("deleteDir(\"%s\") = FALSE\r\n", db_dir_temp);
            rollbackConfiguring();
            return RETURN_NO_ERRORS;
        }

        Ar_Configurator_sendACK(file_code);
        Ar_Configurator_setConnectionMode(CCM_NotConnected);
        if (_bTempDbSaved) {
            _bTempDbSaved = FALSE;

            Ql_FS_Delete(DBFILE_CONFIG(FILENAME_PATTERN_CONFIG_DB_FILE));

            Ar_System_StartDeviceInitialization();
        }
        return RETURN_NO_ERRORS;
    }

    case PC_CONFIGURATION_CHECK_TARGET:
    {
        Ar_Configurator_sendACK(file_code);
        return RETURN_NO_ERRORS;
    }

    case PC_CHECK_DEVICE_DB_VERSION:
    {
        char new_db_version_str[SYSTEM_INFO_STRINGS_LEN + 1] = {0};
        Ql_strncpy(new_db_version_str, (char*)pData->data,
                   pData->datalen <= SYSTEM_INFO_STRINGS_LEN ? pData->datalen : SYSTEM_INFO_STRINGS_LEN);
        const bool db_version_ok = (0 == Ql_strcmp(DB_STRUCTURE_VERSION, new_db_version_str));
        if (db_version_ok) {
            s32 err = 0;
            SystemInfo *pSysInfo = getSystemInfo(&err);
            const char *prev_db_version_str = pSysInfo ? pSysInfo->db_structure_version : "#####";
            OUT_DEBUG_3("Previous DB version: \"%s\", New DB version: \"%s\"\r\n",
                        prev_db_version_str, new_db_version_str);
        } else {
            OUT_DEBUG_3("Incompatible new DB version: \"%s\". The device need \"%s\" version.\r\n"
                        "Uploading aborted.\r\n",
                        new_db_version_str, DB_STRUCTURE_VERSION);
        }
        Ar_Configurator_sendACK(db_version_ok ? file_code : PC_UPLOAD_SETTINGS_FAILED);
        break;
    }

    case PC_UPLOAD_FACTORY_SETTINGS_START:
    case PC_UPLOAD_SETTINGS_START:
    {
        _bFotaRunOne = TRUE;
        _use_factory_settings = (file_code == PC_UPLOAD_FACTORY_SETTINGS_START);
        if (!createTempDBSkeleton_flash()) {
            OUT_DEBUG_1("createTempDB_flash() = FALSE.\r\n");
            rollbackConfiguring();
            return ERR_DB_SKELETON_NOT_CREATED;
        }
        Ar_Configurator_sendACK(file_code);
        return RETURN_NO_ERRORS;
    }

    case PC_UPLOAD_SETTINGS_NORMAL_END:
    {
        OUT_DEBUG_3("Temporary DB was uploaded normally.\r\n");
        Ar_Configurator_sendACK(file_code);
        return RETURN_NO_ERRORS;
    }

    case PC_UPLOAD_FOTA_FILE_NORMAL_END:
    {
        OUT_DEBUG_3("FOTA file was uploaded normally.\r\n");
        Ar_Configurator_sendACK(file_code);

        Ar_System_setFOTA_process_flag(TRUE);
        Ar_System_requestMcuRestart();


        s32 ret = FOTA_Upgrade();

        if ( ret < RETURN_NO_ERRORS ) {
            OUT_DEBUG_1("FOTA_Upgrade() error\r\n");
            Ql_FS_Delete(DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE));
        }

        Ar_System_setFOTA_process_flag(FALSE);
        Ar_System_requestMcuRestart();
        return ret;
    }

    case PC_UPLOAD_SETTINGS_FAILED:
    {
        OUT_DEBUG_1("DB uploading process FAILED.\r\n");
        if (! deleteDir(db_dir_temp)) {     // clear garbage
            OUT_DEBUG_1("deleteDir(\"%s\") = FALSE\r\n", db_dir_temp);
            rollbackConfiguring();
            return ERR_DB_CONFIGURATION_FAILED;
        }
        Ar_Configurator_sendACK(file_code);
        return ERR_DB_CONFIGURATION_FAILED;
    }

    case PC_DOWNLOAD_SETTINGS_START:
    {

#ifndef SEND_ONLY_SQLITE_DB

    //

    u16 db_struct_size = 0;
    u16 pkt_struct_size = 0;
    u16 db_struct_amount = 1;
    u16 pkt_struct_amount = 1;
    u16 full_size_msg = 0;
    u16 items_in_one_msg = 0;
    s32   datalen;
    _RESET_ONE_FILE_TRANSFER_PARAM;
    CurrParam.files_quantity = sizeof(db_filelist)/sizeof(u8*);
    CurrParam.Start_Cycl = TRUE;

    s32 current_file_size = {0}; // size current file

    // loop through a list of files
    for (u8 i = 0; i < sizeof(db_filelist)/sizeof(u8*); ++i) {
        s32 ret = Ql_FS_GetSize(db_filelist[i]);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Ql_FS_GetSize(%s) = %d error\r\n", db_filelist[i], ret);
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return ret;
        }
        else
        {
            current_file_size = ret;

            pkt_struct_size = pkt_max_list[i];
            db_struct_size = obj_list[i];

            //
            switch(db_codelist[i]){
            case OBJ_RZE:
            case OBJ_RAE:
            case OBJ_ETR:
            case OBJ_MASTERBOARD:
            {
                db_struct_amount = 0;
                u8 *read_data =  Ql_MEM_Alloc(current_file_size);
                datalen = readFromDbFile(db_filelist[i], read_data, current_file_size, 0);
                ParentDevice *tbl_ParentDevices = (ParentDevice *)read_data;
                const u16 size = current_file_size / sizeof(ParentDevice);

                for (u16 pos = 0; pos < size; ++pos) {
                    if (db_codelist[i] == tbl_ParentDevices[pos].type)
                    {
                        db_struct_amount++;
                    }
                }
                Ql_MEM_Free(read_data);

                break;
            }
            default:
            {
                db_struct_amount = current_file_size / db_struct_size;
            }
            };

            // count the number of packets in the entire message
            full_size_msg = db_struct_amount * pkt_struct_size;
            items_in_one_msg = (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF) / pkt_struct_size;
            pkt_struct_amount = db_struct_amount / items_in_one_msg;
            if (db_struct_amount % items_in_one_msg)
                pkt_struct_amount += 1;

            CurrParam.all_files_len += pkt_struct_amount;       // count items in all files (count pkt)
            CurrParam.size_file[i] = current_file_size;         // size one file
            CurrParam.parts_in_file[i] = pkt_struct_amount;     // count items in ONE file (count pkt)
            CurrParam.db_struct_size[i] = db_struct_size;       // size item in DB
            CurrParam.pkt_struct_size[i] = pkt_struct_size;     // size item in PKT

            OUT_DEBUG_1("CurrParam. File=%s Size=%d, db_size=%d, pkt_size=%d, item_count=%d (in one msg %d), pkt_datalen=%d, Parts=%d, Total parts=%d \r\n",
                        db_filelist[i],
                        CurrParam.size_file[i],
                        db_struct_size,
                        pkt_struct_size,
                        db_struct_amount,
                        items_in_one_msg,
                        (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF),
                        CurrParam.parts_in_file[i],
                        CurrParam.all_files_len);

        }
    }

    u8 dataOne[2 + SYSTEM_INFO_STRINGS_LEN + 1] = {(u8)(CurrParam.all_files_len >> 8), (u8)CurrParam.all_files_len};
    Ql_strcpy(&dataOne[2], getSystemInfo(NULL)->db_structure_version);

    fillTxBuffer_configurator(file_code, FALSE, dataOne, sizeof(dataOne));
//    fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
    sendBufferedPacket_configurator();
    return RETURN_NO_ERRORS;

#else

        u32 file_size = 0;
        s32 ret = Ql_FS_GetSize(DBFILE(FILENAME_PATTERN_SQLITE_DB_FILE));
        if (ret < 0) {
            OUT_DEBUG_1("Ql_FS_GetSize() = %d error\r\n", ret);
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return ret;
        }
        else
            file_size = ret;

        if (0 == file_size) {
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return ERR_DB_FILE_IS_EMPTY;
        }

        u32 number = file_size / (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF); // get amount of parts to send
        if (file_size % (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF))
            number += 1;

        u8 data[] = {(u8)(number >> 8), (u8)number};
        u8 data[2 + SYSTEM_INFO_STRINGS_LEN + 1] = {(u8)(CurrParam.all_files_len >> 8), (u8)CurrParam.all_files_len};
        Ql_strcpy(&data[2], getSystemInfo(NULL)->db_structure_version);

        fillTxBuffer_configurator(file_code, FALSE, data, sizeof(data));
        sendBufferedPacket_configurator();
        return RETURN_NO_ERRORS;
#endif
    }

    case PC_DOWNLOAD_SETTINGS_ACK:
    {
#ifndef SEND_ONLY_SQLITE_DB

            u16 max_part = CurrParam.parts_in_file[CurrParam.current_file_index];
            OUT_DEBUG_1("CurrParam. FileIndex=%d FileName=%s (%d), pkt part=%d (flag cycl =%d (if 1 - GO, if 0 - STOP))\r\n",
                        CurrParam.current_file_index,
                        db_filelist[CurrParam.current_file_index],
                        db_codelist[CurrParam.current_file_index],
                        CurrParam.current_file_part,
                        CurrParam.Start_Cycl
                        );

            if(CurrParam.Start_Cycl == FALSE)
                return RETURN_NO_ERRORS;


            // first entry point:
            // file index = 0 (diapazon 0..N-1 // N = CurrParam.files_quantity)
            // part = 0 (diapazon 1..N         // N = CurrParam.parts_in_file[CurrParam.current_file_index]


            bool bNext = TRUE;

            // seek next part
            while (bNext)
            {
                if(++CurrParam.current_file_part > (CurrParam.parts_in_file[CurrParam.current_file_index]))
                {
                    CurrParam.current_file_part = 0;
                    if(++CurrParam.current_file_index > CurrParam.files_quantity)
                    {
                      CurrParam.current_file_index = 0;
                      CurrParam.Start_Cycl = FALSE;
                      bNext = FALSE;
                    }
                }
                if(CurrParam.current_file_part > 0)
                    bNext = FALSE;
            }
            // prepare current part
            if(CurrParam.Start_Cycl == TRUE) // part valid
            {
                //
                s32 ret = processDownloadingSettingsToPCSingle();
                if (ret < RETURN_NO_ERRORS) {
                    OUT_DEBUG_1("processDownloadingDBtoPC() = %d error\r\n", ret);
                    return ret;
                }
            }
            else // not next part
            {
                _RESET_ONE_FILE_TRANSFER_PARAM;
                fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_NORMAL_END, FALSE, NULL, 0); // finalize the downloading process
                sendBufferedPacket_configurator();
            }

        return RETURN_NO_ERRORS;
#else
        s32 ret = processDownloadingSettingsToPC(FALSE);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("processDownloadingDBtoPC() = %d error\r\n", ret);
            return ret;
        }
        return RETURN_NO_ERRORS;
#endif
    }

    case PC_DOWNLOAD_SETTINGS_NORMAL_END:
    {
        OUT_DEBUG_3("Temporary DB was downloaded normally.\r\n");
        Ar_Configurator_sendACK(PC_DOWNLOAD_SETTINGS_NACK);
        return RETURN_NO_ERRORS;
    }

    case PC_DOWNLOAD_SETTINGS_FAILED:
    {
        OUT_DEBUG_1("DB downloading process FAILED.\r\n");
        processDownloadingSettingsToPC(TRUE); // reset
        Ar_Configurator_sendACK(file_code);
        return ERR_DB_CONFIGURATION_FAILED;
    }

    case PC_READ_LAST_TOUCHMEMORY_CODE:
    {
        u16 etr_uin = pData->data[TMVPKT_ETR_UIN_H] << 8 | pData->data[TMVPKT_ETR_UIN_L];

        if (!isParentDeviceConnected(etr_uin, OBJ_ETR)) {
            OUT_DEBUG_1("Unit %s #%d is not connected. Read key failed.\r\n",
                        getUnitTypeByCode(OBJ_ETR), etr_uin);
            Ar_Configurator_sendACK(PC_LAST_TOUCHMEMORY_CODE_FAILED);
            return RETURN_NO_ERRORS;
        }

        s32 ret = requestLastReceivedTouchMemoryCode(etr_uin);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("requestLastReceivedTouchMemoryCode() = %d error\r\n", ret);
            Ar_Configurator_sendACK(PC_LAST_TOUCHMEMORY_CODE_FAILED);
            return ret;
        }

        Ar_Configurator_sendACK(file_code);
        return RETURN_NO_ERRORS;
    }

    case PC_LAST_TOUCHMEMORY_CODE_FAILED:
    {
        clearETRsBlocking();
        Ar_Configurator_sendACK(file_code);
        return RETURN_NO_ERRORS;
    }

    case PC_READ_ADC:
    {
        Ar_Power_requestAdcData();
        return RETURN_NO_ERRORS;
    }

    default:
    {
        s32 ret = saveSettingsToTemporaryDB(pData);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("saveSettingsToTemporaryDB() = %d error.\r\n", ret);
            rollbackConfiguring();
            return ret;
        }
    }
    }

    return RETURN_NO_ERRORS;
}


//static inline void unpackBytesToFlags(u8 *in_bytesArray, u16 hByteIdx, u16 lByteIdx, bool out_bitsArray[])
//{
//    // -- unpack bytes to bits (i.e. flags) (MS Bit must be placed at MAX index pos of the array)
//    u16 bytes_qty = hByteIdx - lByteIdx + 1;
//    for (s32 j = bytes_qty - 1; j >= 0; --j) {
//        u8 byte = in_bytesArray[hByteIdx + j];
//        for (s8 i = 7; i >= 0; --i) {
//            if (j * 8 + i < sizeof(out_bitsArray))
//                out_bitsArray[j * 8 + i] = byte & (1 << i);
//        }
//    }
//}

//static inline void packFlagsToBytes(void)
//{
    // -- pack BOOL flags to bytes (MS Bit placed at MAX index pos of the vector)
//    int checkboxes_bytes_qty = CSPKT_CHECKBOXES_H - CSPKT_CHECKBOXES_L + 1;     SAMPLE CODE
//    for (int j = checkboxes_bytes_qty - 1; j >= 0; --j) {
//        quint8 byte = 0;
//        for (int i = 7; i >= 0; --i) {
//            if (j * 8 + i < item->_checkboxes.size())
//                byte |= item->_checkboxes.at(j * 8 + i) ? 1 : 0;
//            byte <<= 1;
//        }
//        temp[CSPKT_CHECKBOXES_H + j] = byte;
//    }
//}


// pDataStream points to the UART-buffer => its size is less then or equal to 1024 bytes
s32 saveSettingsToTemporaryDB(RxConfiguratorPkt *pPkt)
{
    OUT_DEBUG_2("saveSettingsToTemporaryDB()\r\n");

    DbObjectCode file_code = pPkt->dataType;    // file code <=> DB table name <=> data_type

    //bool bAppendToFile = pPkt->bAppend;  always will appended (see SAVING_TEMP_DB_TABLE_END macro for details)

    switch (file_code) {
    case OBJ_MASTERBOARD:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(ParentDevice, MBPKT_MAX)

            b->parent_id =  COPY_U16(MBPKT_PARENT_ID_H, MBPKT_PARENT_ID_L);
            b->type = file_code;
            b->uin = 0; //COPY_U16(MBPKT_UIN_H, MBPKT_UIN_L);
            b->id = COPY_U16(MBPKT_ID_H, MBPKT_ID_L);

            b->connection_state.counter = 0;
            b->connection_state.value = UnitAfterPowerOn;

            COPY_ALIAS(d.masterboard.alias, MBPKT_ALIAS_H);
            b->d.masterboard.bMcuApproved = FALSE;
            b->d.masterboard.bMcuConnectionRequested = FALSE;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_PARENT_DEVS )
        break;
    }

    case OBJ_ETR:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(ParentDevice, ETRPKT_MAX)

            b->parent_id =  COPY_U16(ETRPKT_PARENT_ID_H, ETRPKT_PARENT_ID_L);
            b->type = file_code;
            b->uin = COPY_U16(ETRPKT_UIN_H,ETRPKT_UIN_L);
            b->id = COPY_U16(ETRPKT_ID_H, ETRPKT_ID_L);
            b->d.etr.type = COPY_U8(ETRPKT_ETR_TYPE);
//            b->type = COPY_U8(ETRPKT_ETR_TYPE);

            b->connection_state.counter = 0;
            b->connection_state.value = UnitAfterPowerOn;

            COPY_ALIAS(d.etr.alias, ETRPKT_ALIAS_H);

            for (u8 i = 0; i < TOUCHMEMORY_MAX_KEY_SIZE; ++i)
                b->d.etr.last_received_keycode[i] = 0;

            b->d.etr.bBlocked = FALSE;

            /* totally 100 zones (10x10) on ETR's LED-indicator */
            b->d.etr.led_indicator.current_led_decade = 0;
            for (u8 row = 0; row < 10; ++row) {
                b->d.etr.led_indicator.led_decade_state_mask_high[row] = 0x30;
                for (u8 col = 0; col < 10; ++col)
                    b->d.etr.led_indicator.led_decade_state_masks_low[row][col] = 0x30;
            }

            b->d.etr.buzzer.state = UnitStateOff;
            BehaviorPreset bp = {0};
            b->d.etr.buzzer.behavior_preset = bp;
            b->d.etr.buzzer.puin = COPY_U16(ETRPKT_UIN_H,ETRPKT_UIN_L);
            b->d.etr.buzzer.counter=0;

            b->d.etr.related_groups_qty = COPY_U16(ETRPKT_RELATED_GROUPS_QTY_H,
                                                   ETRPKT_RELATED_GROUPS_QTY_L);
            b->d.etr.tbl_related_groups.size = 0;
            b->d.etr.tbl_related_groups.t = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_PARENT_DEVS )
        break;
    }

    case OBJ_RZE:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(ParentDevice, EPKT_MAX)

            b->parent_id =  COPY_U16(EPKT_PARENT_ID_H, EPKT_PARENT_ID_L);
            b->type = file_code;
            b->uin = COPY_U16(EPKT_UIN_H, EPKT_UIN_L);
            b->id = COPY_U16(EPKT_ID_H, EPKT_ID_L);

            b->connection_state.counter = 0;
            b->connection_state.value = UnitAfterPowerOn;

            COPY_ALIAS(d.zone_expander.alias, EPKT_ALIAS_H);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_PARENT_DEVS )
        break;
    }

    case OBJ_RAE:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(ParentDevice, EPKT_MAX)

            b->parent_id =  COPY_U16(EPKT_PARENT_ID_H, EPKT_PARENT_ID_L);
            b->type = file_code;
            b->uin = COPY_U16(EPKT_UIN_H, EPKT_UIN_L);
            b->id = COPY_U16(EPKT_ID_H, EPKT_ID_L);

            b->connection_state.counter = 0;
            b->connection_state.value = UnitAfterPowerOn;

            COPY_ALIAS(d.relay_expander.alias, EPKT_ALIAS_H);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_PARENT_DEVS )
        break;
    }

    case OBJ_KEYBOARD_CODE:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(KeyboardCode, KBDCPKT_MAX)

            b->id =  COPY_U16(KBDCPKT_ID_H, KBDCPKT_ID_L);
            b->len = COPY_U8(KBDCPKT_KEY_SIZE);
            b->action = COPY_U8(KBDCPKT_ACTION);
            b->group_id = COPY_U16(KBDCPKT_ARMING_GROUP_ID_H, KBDCPKT_ARMING_GROUP_ID_L);

            for (u8 i = 0; i < KEYBOARD_MAX_KEY_SIZE; ++i) {
                b->value[i] = (i < pData[KBDCPKT_KEY_SIZE]) ? pData[KBDCPKT_KEY_H + i] : 0;
            }

            COPY_ALIAS(alias, KBDCPKT_ALIAS_H);

            for (u8 i = 0; i < (sizeof(b->events) / sizeof(Event *)); ++i)
                b->events[i] = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_KEYBOARD_CODES )
        break;
    }

    case OBJ_TOUCHMEMORY_CODE:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(TouchmemoryCode, TMCPKT_MAX)

            b->id =  COPY_U16(TMCPKT_ID_H, TMCPKT_ID_L);
            b->len = COPY_U8(TMCPKT_KEY_SIZE);
            b->action = COPY_U8(TMCPKT_ACTION);
            b->group_id = COPY_U16(TMCPKT_ARMING_GROUP_ID_H, TMCPKT_ARMING_GROUP_ID_L);

            for (u8 i = 0; i < TOUCHMEMORY_MAX_KEY_SIZE; ++i) {
                b->value[i] = (i < pData[TMCPKT_KEY_SIZE]) ? pData[TMCPKT_KEY_H + i] : 0;
            }

            COPY_ALIAS(alias, TMCPKT_ALIAS_H);

            for (u8 i = 0; i < (sizeof(b->events) / sizeof(Event *)); ++i)
                b->events[i] = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_TOUCHMEMORY_CODES )
        break;
    }

    case OBJ_ARMING_GROUP:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(ArmingGroup, AGPKT_MAX)

            b->id = COPY_U16(AGPKT_ID_H, AGPKT_ID_L);
            COPY_ALIAS(alias, AGPKT_ALIAS_H);

            b->p_group_state = NULL;
            b->p_prev_group_state = NULL;
            b->arming_zoneDelay_counter = 0;
            b->disarming_zoneDelay_counter = 0;
            b->entry_delay_zone = NULL;
            b->delayed_zones = NULL;

            for (u8 i = 0; i < (sizeof(b->events) / sizeof(Event *)); ++i)
                b->events[i] = NULL;

            b->related_ETRs_qty = COPY_U16(AGPKT_RELATED_ETRs_QTY_H, AGPKT_RELATED_ETRs_QTY_L);
            b->tbl_related_ETRs.size = 0;
            b->tbl_related_ETRs.t = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_ARMING_GROUPS )
        break;
    }

    case OBJ_RELATION_ETR_AGROUP:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(RelationETRArmingGroup, REAGPKT_MAX)

            b->ETR_id = COPY_U16(REAGPKT_ETR_ID_H, REAGPKT_ETR_ID_L);
            b->group_id = COPY_U16(REAGPKT_GROUP_ID_H, REAGPKT_GROUP_ID_L);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_RELATIONS_ETR_AGROUP )
        break;
    }

    case OBJ_ZONE:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Zone, ZPKT_MAX)

            b->id = COPY_U16(ZPKT_ID_H, ZPKT_ID_L);
            b->humanized_id = COPY_U16(ZPKT_HUMANIZED_ID_H, ZPKT_HUMANIZED_ID_L);
            COPY_ALIAS(alias, ZPKT_ALIAS_H);
            b->suin = COPY_U8(ZPKT_SUIN);
            b->enabled = COPY_BOOL(ZPKT_ENABLE_FLAG);
            b->zone_type = COPY_TYPE_EX(ZoneType, ZPKT_ZONE_TYPE);
            b->group_id = COPY_U16(ZPKT_ARMING_GROUP_ID_H, ZPKT_ARMING_GROUP_ID_L);
            b->damage_notification_enabled = COPY_BOOL(ZPKT_LOOP_DAMAGE_NOTIFICATION_FLAG);

            b->ptype = COPY_TYPE(ZPKT_PARENTDEV_TYPE);
            b->puin = OBJ_MASTERBOARD == b->ptype ? 0 : COPY_U16(ZPKT_PARENTDEV_UIN_H, ZPKT_PARENTDEV_UIN_L);

            b->parent_device = NULL;
            b->zoneDamage_counter = 0;
            b->last_event_type = 0;

            for (u8 i = 0; i < (sizeof(b->events) / sizeof(Event *)); ++i)
                b->events[i] = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_ZONES )
        break;
    }

    case OBJ_RELAY:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Relay, RPKT_MAX)

            b->id = COPY_U16(RPKT_ID_H, RPKT_ID_L);
            COPY_ALIAS(alias, RPKT_ALIAS_H);
            b->suin = COPY_U8(RPKT_SUIN);
            b->enabled = COPY_BOOL(RPKT_ENABLE_FLAG);
            b->remote_control = COPY_BOOL(RPKT_REMOTE_CONTROL_FLAG);
            b->notify_on_state_changed = COPY_BOOL(RPKT_NOTIFY_ON_STATE_CHANGED_FLAG);
            b->humanized_id = COPY_U16(RPKT_HUMANIZED_ID_H, RPKT_HUMANIZED_ID_L);

            b->ptype = COPY_TYPE(RPKT_PARENTDEV_TYPE);
            b->puin = OBJ_MASTERBOARD == b->ptype ? 0 : COPY_U16(RPKT_PARENTDEV_UIN_H, RPKT_PARENTDEV_UIN_L);

            b->parent_device = NULL;
            b->state = UnitStateOff;

            BehaviorPreset bp = {0};
            b->behavior_preset = bp;

            b->load_type = COPY_U8(RPKT_LOAD_TYPE);
            b->group_id = COPY_U16(RPKT_GROUP_ID_H, RPKT_GROUP_ID_L);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_RELAYS )
        break;
    }

    case OBJ_LED:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Led, LEDPKT_MAX)

            b->id = COPY_U16(LEDPKT_ID_H, LEDPKT_ID_L);
            COPY_ALIAS(alias, LEDPKT_ALIAS_H);
            b->suin = COPY_U8(LEDPKT_SUIN);
            b->enabled = COPY_BOOL(LEDPKT_ENABLE_FLAG);
            b->humanized_id = COPY_U16(LEDPKT_HUMANIZED_ID_H, LEDPKT_HUMANIZED_ID_L);

            b->ptype = COPY_TYPE(LEDPKT_PARENTDEV_TYPE);
            b->puin = OBJ_MASTERBOARD == b->ptype ? 0 : COPY_U16(LEDPKT_PARENTDEV_UIN_H, LEDPKT_PARENTDEV_UIN_L);

            b->parent_device = NULL;
            b->state = UnitStateOff;

            BehaviorPreset bp = {0};
            b->behavior_preset = bp;

            b->b_arming_led = COPY_BOOL(LEDPKT_ARMING_LED_FLAG);
            b->group_id = COPY_U16(LEDPKT_GROUP_ID_H, LEDPKT_GROUP_ID_L);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_LEDS )
        break;
    }

    case OBJ_BELL:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Bell, BELLPKT_MAX)

            b->id = COPY_U16(BELLPKT_ID_H, BELLPKT_ID_L);
            COPY_ALIAS(alias, BELLPKT_ALIAS_H);
            b->suin = COPY_U8(BELLPKT_SUIN);
            b->enabled = COPY_BOOL(BELLPKT_ENABLE_FLAG);
            b->remote_control = COPY_BOOL(BELLPKT_REMOTE_CONTROL_FLAG);
            b->humanized_id = COPY_U16(BELLPKT_HUMANIZED_ID_H, BELLPKT_HUMANIZED_ID_L);

            b->ptype = COPY_TYPE(BELLPKT_PARENTDEV_TYPE);
            b->puin = OBJ_MASTERBOARD == b->ptype ? 0 : COPY_U16(BELLPKT_PARENTDEV_UIN_H, BELLPKT_PARENTDEV_UIN_L);

            b->parent_device = NULL;
            b->state = UnitStateOff;

            BehaviorPreset bp = {0};
            b->behavior_preset = bp;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_BELLS )
        break;
    }

    case OBJ_BUTTON:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Button, BTNPKT_MAX)

            b->id = COPY_U16(BTNPKT_ID_H, BTNPKT_ID_L);
            COPY_ALIAS(alias, BTNPKT_ALIAS_H);
            b->suin = COPY_U8(BTNPKT_SUIN);
            b->enabled = COPY_BOOL(BTNPKT_ENABLE_FLAG);
            b->humanized_id = COPY_U16(BTNPKT_HUMANIZED_ID_H, BTNPKT_HUMANIZED_ID_L);

            b->ptype = COPY_TYPE(BTNPKT_PARENTDEV_TYPE);
            b->puin = OBJ_MASTERBOARD == b->ptype ? 0 : COPY_U16(BTNPKT_PARENTDEV_UIN_H, BTNPKT_PARENTDEV_UIN_L);

            b->parent_device = NULL;

            for (u8 i = 0; i < (sizeof(b->events) / sizeof(Event *)); ++i)
                b->events[i] = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_BUTTONS )
        break;
    }

    case OBJ_EVENT:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Event, EVTPKT_MAX)

            b->id = COPY_U16(EVTPKT_ID_H, EVTPKT_ID_L);
            COPY_ALIAS(alias, EVTPKT_ALIAS_H);
            b->emitter_id = COPY_U16(EVTPKT_EMITTER_ID_H, EVTPKT_EMITTER_ID_L);
            b->emitter_type = COPY_TYPE(EVTPKT_EMITTER_TYPE);
            b->event_code = COPY_U8(EVTPKT_EVENT);

            b->subscribers_qty = COPY_U8(EVTPKT_SUBSCRIBERS_QTY);
            b->tbl_subscribers.size = 0;
            b->tbl_subscribers.t = NULL;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_EVENTS )
        break;
    }

    case OBJ_REACTION:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(Reaction, RCNPKT_MAX)

            b->id = COPY_U16(RCNPKT_ID_H, RCNPKT_ID_L);
            COPY_ALIAS(alias, RCNPKT_ALIAS_H);
            b->behavior_preset_id = COPY_U16(RCNPKT_BEHAVIOR_PRESET_ID_H, RCNPKT_BEHAVIOR_PRESET_ID_L);
            b->event_id = COPY_U16(RCNPKT_EVENT_ID_H, RCNPKT_EVENT_ID_L);

            b->performer_behavior = COPY_U8(RCNPKT_PERFORMER_BEHAVIOR);
            b->performer_id = COPY_U16(RCNPKT_PERFORMER_ID_H, RCNPKT_PERFORMER_ID_L);
            b->performer_type = COPY_TYPE(RCNPKT_PERFORMER_TYPE);

            for (u8 i = 0; i < STATE_MAX; ++i)
                b->valid_states[i] = pData[RCNPKT_VALID_STATES_H + i];

            b->bReversible = COPY_BOOL(RCNPKT_REVERSIBLE_FLAG);
            b->reverse_behavior = COPY_U8(RCNPKT_REVERSE_PERFORMER_BEHAVIOR);
            b->reverse_preset_id = COPY_U16(RCNPKT_REVERSE_BEHAVIOR_PRESET_ID_H,
                                            RCNPKT_REVERSE_BEHAVIOR_PRESET_ID_L);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_REACTIONS )
        break;
    }

    case OBJ_BEHAVIOR_PRESET:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(BehaviorPreset, BHPPKT_MAX)

            b->id = COPY_U16(BHPPKT_ID_H, BHPPKT_ID_L);
            COPY_ALIAS(alias, BHPPKT_ALIAS_H);
            b->pulses_in_batch = COPY_U8(BHPPKT_PULSES_IN_BATCH);
            b->pulse_len = COPY_U8(BHPPKT_PULSE_LEN);
            b->pause_len = COPY_U8(BHPPKT_PAUSE_LEN);
            b->batch_pause_len = COPY_U8(BHPPKT_BATCH_PAUSE_LEN);
            b->behavior_type = COPY_U8(BHPPKT_BEHAVIOR_TYPE);

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_BEHAVIOR_PRESETS )
        break;
    }

    case OBJ_COMMON_SETTINGS:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(CommonSettings, CSPKT_MAX)

            // phonelist aux
            b->aux_phone_list.size = 0;
            b->aux_phone_list.phones = NULL;
            b->aux_phone_list._size_m = 0;
            b->aux_phone_list._phones_m = NULL;

            // common
            b->codec_type = COPY_TYPE_EX(PultMessageCodecType, CSPKT_CODEC_TYPE);
            b->entry_delay_sec = COPY_U16(CSPKT_ENTRY_DELAY_H, CSPKT_ENTRY_DELAY_L);
            b->arming_delay_sec = COPY_U16(CSPKT_ARMING_DELAY_H, CSPKT_ARMING_DELAY_L);
            b->debug_level = COPY_U8(CSPKT_DEBUG_LEVEL);
            b->aux_phohelist_default_size = COPY_U8(CSPKT_AUX_PHONELIST_DEFAULT_SIZE);

            b->p_masterboard = NULL;

            b->bMainBellOk = FALSE;
            b->bSystemTrouble = FALSE;

            b->dtmf_pulse_ms = COPY_U16(CSPKT_DTMF_PULSE_MS_H, CSPKT_DTMF_PULSE_MS_L);
            b->dtmf_pause_ms = COPY_U16(CSPKT_DTMF_PAUSE_MS_H, CSPKT_DTMF_PAUSE_MS_L);

            b->adc_a = COPY_U16(CSPKT_ADC_A_MANTISSA_H, CSPKT_ADC_A_MANTISSA_L) +
                        (float)COPY_U16(CSPKT_ADC_A_DECIMAL_H, CSPKT_ADC_A_DECIMAL_L) / CSPKT_ADC_Precision;
            b->adc_b = COPY_U16(CSPKT_ADC_B_MANTISSA_H, CSPKT_ADC_B_MANTISSA_L) +
                        (float)COPY_U16(CSPKT_ADC_B_DECIMAL_H, CSPKT_ADC_B_DECIMAL_L) / CSPKT_ADC_Precision;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_COMMON_SETTINGS )
        break;
    }

    case OBJ_SIM_CARD:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(SimSettings, SIMPKT_MAX)

            b->id = COPY_U8(SIMPKT_ID);
            // common
            b->allow_sms_to_pult = COPY_U8(SIMPKT_ALLOW_SMS);
            b->can_be_used = COPY_U8(SIMPKT_USABLE);
            b->prefer_gprs = COPY_U8(SIMPKT_PREFER_GPRS);
            b->voicecall_attempts_qty = COPY_U8(SIMPKT_VOICECALL_ATTEMPTS_QTY);
            b->gprs_attempts_qty = COPY_U8(SIMPKT_GPRS_ATTEMPTS_QTY);
            // UDP
            b->udp_ip_list.dest_port = COPY_U16(SIMPKT_UDP_DESTPORT_H, SIMPKT_UDP_DESTPORT_L);
            b->udp_ip_list.local_port = COPY_U16(SIMPKT_UDP_LOCALPORT_H, SIMPKT_UDP_LOCALPORT_L);
            b->udp_ip_list._size_m = 0;
            b->udp_ip_list._IPs_m = NULL;
            b->udp_ip_list.size = 0;
            b->udp_ip_list.IPs = NULL;
            // TCP
            b->tcp_ip_list.dest_port = COPY_U16(SIMPKT_TCP_DESTPORT_H, SIMPKT_TCP_DESTPORT_L);
            b->tcp_ip_list.local_port = COPY_U16(SIMPKT_TCP_LOCALPORT_H, SIMPKT_TCP_LOCALPORT_L);
            b->tcp_ip_list._size_m = 0;
            b->tcp_ip_list._IPs_m = NULL;
            b->tcp_ip_list.size = 0;
            b->tcp_ip_list.IPs = NULL;
            // VOICE CALL
            b->phone_list._size_m = 0;
            b->phone_list._phones_m = NULL;
            b->phone_list.size = 0;
            b->phone_list.phones = NULL;

            for (u8 i = 0; i <= APN_LEN; ++i) {
                b->apn[i] = (i < APN_LEN) ? pData[SIMPKT_APN_H + i] : 0;
            }

            for (u8 i = 0; i <= CREDENTIALS_LEN; ++i) {
                b->login[i]    = (i < CREDENTIALS_LEN) ? pData[SIMPKT_LOGIN_H + i] : 0;
                b->password[i] = (i < CREDENTIALS_LEN) ? pData[SIMPKT_PASS_H + i] : 0;
            }

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_SIMCARDS )
        break;
    }

    case OBJ_PHONELIST_SIM1:
    case OBJ_PHONELIST_SIM2:
    case OBJ_PHONELIST_AUX:
    {
        char name[30] = {0};

        switch (file_code) {
        case OBJ_PHONELIST_SIM1:
            Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM1);
            break;
        case OBJ_PHONELIST_SIM2:
            Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM2);
            break;
        case OBJ_PHONELIST_AUX:
            Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_AUX);
            break;
        default:
            break;
        }

        SAVING_TEMP_DB_TABLE_BEGIN(PhoneNumber, PHNPKT_MAX)

            b->id = pData[PHNPKT_ID];
            b->allowed_simcard = pData[PHNPKT_ALLOWED_SIMCARD];

            for (u8 i = 0; i <= PHONE_LEN; ++i) {
                b->number[i] = (i < PHONE_LEN) ? pData[PHNPKT_PHONENUMBER_H + i] : 0;
            }

        SAVING_TEMP_DB_TABLE_END( name )
        break;
    }

    case OBJ_IPLIST_SIM1:
    case OBJ_IPLIST_SIM2:
    {
        char name[30] = {0};
        Ql_strcpy(name, OBJ_IPLIST_SIM1 == file_code
                                ? FILENAME_PATTERN_IPLIST_SIM1
                                : FILENAME_PATTERN_IPLIST_SIM2);

        SAVING_TEMP_DB_TABLE_BEGIN(IpAddress, IPAPKT_MAX)

            b->id = pData[IPAPKT_ID];
            b->address[0] = pData[IPAPKT_IPADDRESS_0];
            b->address[1] = pData[IPAPKT_IPADDRESS_1];
            b->address[2] = pData[IPAPKT_IPADDRESS_2];
            b->address[3] = pData[IPAPKT_IPADDRESS_3];

        SAVING_TEMP_DB_TABLE_END( name )
        break;
    }

    case OBJ_SQLITE_DB_FILE:
    {
        /* will create a byte array in the HEAP with a size equal to the received datablock */
        SAVING_TEMP_DB_TABLE_BEGIN(u8, sizeof(u8));

            *b = *pData;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_SQLITE_DB_FILE )
        break;
    }


    case OBJ_FOTA_FILE:
    {

        if(_bFotaRunOne)
        {
            _bFotaRunOne = FALSE;


            Ql_FS_Delete(DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE));

            // create DIR
            s32 ret_dir = Ql_FS_CheckDir(DB_DIR_FOTA);
            if (QL_RET_ERR_FILENOTFOUND == ret_dir) {

                    if (!createDir(DB_DIR_FOTA))
                    {
                        return ERR_DB_SAVING_FAILED;
                    }
            }

            // create FILE
            s32 fh = Ql_FS_Open(DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE), QL_FS_CREATE_ALWAYS);
            if (fh < QL_RET_OK) {
                OUT_DEBUG_1("Error %d occured while creating or opening FOTA-file \"%s\".\r\n",
                            fh, DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE));
                return ERR_DB_SAVING_FAILED;
            }
            s32 ret = Ql_FS_Truncate(fh);
            if (ret < QL_RET_OK) {
                OUT_DEBUG_1("Error %d occured while truncating FOTAfile \"%s\".\r\n",
                            ret, DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE));
                return ERR_DB_SAVING_FAILED;
            }
            Ql_FS_Close(fh);
            OUT_DEBUG_3("File \"%s\" created.\r\n", DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE));

        }

        /* will create a byte array in the HEAP with a size equal to the received datablock */
        SAVING_FOTA_FILE_BEGIN(u8, sizeof(u8));

            *b = *pData;

        SAVING_FOTA_FILE_END( FILENAME_PATTERN_FOTA_DB_FILE )
        break;
    }


    case OBJ_SYSTEM_INFO:
    {
        SAVING_TEMP_DB_TABLE_BEGIN(SystemInfo, SINFPKT_MAX)

            for (u8 i = 0; i <= SYSTEM_INFO_STRINGS_LEN; ++i) {
                b->db_structure_version[i] = (i < SYSTEM_INFO_STRINGS_LEN)
                                                     ? pData[SINFPKT_SETTINGS_IDENT_STR_H + i]
                                                     : 0;
            }

            b->settings_signature = pData[SINFPKT_SETTINGS_SIGNATURE_3] << 24
                                        | pData[SINFPKT_SETTINGS_SIGNATURE_2] << 16
                                        | pData[SINFPKT_SETTINGS_SIGNATURE_1] << 8
                                        | pData[SINFPKT_SETTINGS_SIGNATURE_0];

            b->settings_signature_autosync = 0;

            b->firmware_version_major = ARMOR_FW_VERSION_MAJOR;
            b->firmware_version_minor = ARMOR_FW_VERSION_MINOR;

        SAVING_TEMP_DB_TABLE_END( FILENAME_PATTERN_SYSTEMINFO )
        break;
    }

    default:
        OUT_DEBUG_1("ERROR: Object code \"%d\" is not registered in the system. "
                    "Nothing will be written.\r\n",
                    file_code);
        return ERR_UNREGISTERED_DB_OBJ_TYPE;
    }

    Ar_Configurator_sendACK(PC_UPLOAD_SETTINGS_ACK);
    return RETURN_NO_ERRORS;
}

//
s32  saveSettingsToAutosyncTemporaryDB(RxConfiguratorPkt *pPkt)
{
    OUT_DEBUG_2("saveSettingsToAutosyncTemporaryDB()\r\n");

    DbObjectCode file_code = pPkt->dataType;

    switch (file_code) {
    case OBJ_PHONELIST_SIM1:
    case OBJ_PHONELIST_SIM2:
    {
        char name[30] = {0};

        switch (file_code) {
        case OBJ_PHONELIST_SIM1:
            Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM1);
            break;
        case OBJ_PHONELIST_SIM2:
            Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM2);
            break;
        default:
            break;
        }

        SAVING_AUTOSYNC_TEMP_DB_TABLE_BEGIN(PhoneNumber, PHNPKT_MAX)

            b->id = pData[PHNPKT_ID];
            b->allowed_simcard = pData[PHNPKT_ALLOWED_SIMCARD];

            for (u8 i = 0; i <= PHONE_LEN; ++i) {
                b->number[i] = (i < PHONE_LEN) ? pData[PHNPKT_PHONENUMBER_H + i] : 0;
            }

        SAVING_AUTOSYNC_TEMP_DB_TABLE_END( name )
        break;
    }
    case OBJ_IPLIST_SIM1:
    case OBJ_IPLIST_SIM2:
    {
        char name[30] = {0};
        Ql_strcpy(name, OBJ_IPLIST_SIM1 == file_code
                                ? FILENAME_PATTERN_IPLIST_SIM1
                                : FILENAME_PATTERN_IPLIST_SIM2);

        SAVING_AUTOSYNC_TEMP_DB_TABLE_BEGIN(IpAddress, IPAPKT_MAX)

            b->id = pData[IPAPKT_ID];
            b->address[0] = pData[IPAPKT_IPADDRESS_0];
            b->address[1] = pData[IPAPKT_IPADDRESS_1];
            b->address[2] = pData[IPAPKT_IPADDRESS_2];
            b->address[3] = pData[IPAPKT_IPADDRESS_3];

        SAVING_AUTOSYNC_TEMP_DB_TABLE_END( name )
        break;
    }
    case OBJ_COMMON_AUTOSYNC_SETTINGS:

        break;
    case OBJ_SYSTEM_INFO:

        break;
    default:
        OUT_DEBUG_1("ERROR: Object code \"%d\" is not registered in the system. "
                    "Nothing will be written.\r\n",
                    file_code);
        return ERR_UNREGISTERED_DB_OBJ_TYPE;
    }

    Ar_Configurator_sendACK(PC_UPLOAD_SETTINGS_ACK);
    return RETURN_NO_ERRORS;
}

//
s32 processDownloadingSettingsToPC(bool reset)
{
    OUT_DEBUG_2("processDownloadingSettingsToPC()\r\n");

    static bool bAppend = FALSE;
    static u32 current_offset = 0;
    static u32 file_size = 0;
    u8 data[TEMP_TX_BUFFER_SIZE] = {0};
    s32 datalen = 0;
    s32 ret = 0;

    if (reset) {
        file_size = 0;
        current_offset = 0;
        bAppend = FALSE;
        return RETURN_NO_ERRORS;
    }

    if (! bAppend) {
        ret = Ql_FS_GetSize(DBFILE(FILENAME_PATTERN_SQLITE_DB_FILE));
        if (ret < 0) {
            OUT_DEBUG_1("Ql_FS_GetSize() = %d error\r\n", ret);
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return ret;
        }
        else
            file_size = ret;

        if (0 == file_size) {
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return ERR_DB_FILE_IS_EMPTY;
        }
    }

    u32 least = file_size - current_offset;
    u16 mustread = least > (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF) ? (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF) : least;

    if (mustread) {
        datalen = readFromDbFile(DBFILE(FILENAME_PATTERN_SQLITE_DB_FILE), data, mustread, current_offset);

        if (datalen < 0) {
            OUT_DEBUG_1("readFromDbFile() = %d error\r\n", datalen);
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return datalen;
        } else if (datalen < mustread) {
            OUT_DEBUG_1("readFromDbFile(): can't read sqlite_db_file\r\n");
            fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
            sendBufferedPacket_configurator();
            return ERR_READ_DB_FILE_FAILED;
        }

        fillTxBuffer_configurator(OBJ_SQLITE_DB_FILE, bAppend, data, datalen);  // next block of the SQLITE DB file
        sendBufferedPacket_configurator();

//OUT_DEBUG_8("file_size = %d, current_offset = %d, mustread = %d, really read = %d\r\n", file_size, current_offset, mustread, datalen);

        current_offset += datalen;
        bAppend = TRUE;
    } else {
        file_size = 0;
        current_offset = 0;
        bAppend = FALSE;

        fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_NORMAL_END, FALSE, NULL, 0); // finalize the downloading process
        sendBufferedPacket_configurator();
    }

    return RETURN_NO_ERRORS;
}


//
s32 processDownloadingSettingsToPCSingle()
{
    OUT_DEBUG_2("processDownloadingSettingsToPCSingle()\r\n");
    static bool bAppend;
    u8 data[TEMP_TX_BUFFER_SIZE] = {0};
    u16 size_file = CurrParam.size_file[CurrParam.current_file_index];
    u16 items_in_file = size_file / CurrParam.db_struct_size[CurrParam.current_file_index];
    u16 items_in_one_msg = (TEMP_TX_BUFFER_SIZE - SPS_PREF_N_SUFF) / CurrParam.pkt_struct_size[CurrParam.current_file_index];
    u16 db_offset;
    u16 pkt_offset;
    u16 pkt_count;
    u16 pkt_count_iteration;

        bAppend = CurrParam.current_file_part == 1 ? FALSE : TRUE;

      // open file
        u8 *read_data =  Ql_MEM_Alloc(size_file);
        s32 datalen = 0;
        datalen = readFromDbFile(db_filelist[CurrParam.current_file_index], read_data, size_file, 0);
                    if (datalen < 0) {
                        Ql_MEM_Free(read_data);
                        OUT_DEBUG_1("readFromDbFile() = %d error\r\n", datalen);
                        fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
                        sendBufferedPacket_configurator();
                        return datalen;
                    }
                    else if (datalen < size_file)
                    {
                        Ql_MEM_Free(read_data);
                        OUT_DEBUG_1("readFromDbFile(): can't read %s",
                                    db_filelist[CurrParam.current_file_index]);
                        fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
                        sendBufferedPacket_configurator();
                        return ERR_READ_DB_FILE_FAILED;
                    }
      // main cycl

        u16 work_part_from = (CurrParam.current_file_part - 1) * items_in_one_msg + 1;
        u16 work_part_to = CurrParam.current_file_part * items_in_one_msg;

        OUT_DEBUG_2("items_in_file=%d, work_part_from=%d, work_part_to=%d \r\n" ,items_in_file ,  work_part_from, work_part_to );




        if(db_codelist[CurrParam.current_file_index] == OBJ_MASTERBOARD || db_codelist[CurrParam.current_file_index] == OBJ_RZE || db_codelist[CurrParam.current_file_index] == OBJ_RAE || db_codelist[CurrParam.current_file_index] == OBJ_ETR  )
        {
            pkt_count_iteration = 0;
            pkt_count = 0;

            ParentDevice *tbl_ParentDevices = (ParentDevice *)read_data;
            const u16 size = size_file / sizeof(ParentDevice);

            for (u16 pos = 0; pos < size; ++pos)
            {
                if (db_codelist[CurrParam.current_file_index] == tbl_ParentDevices[pos].type)
                {
                    if(pkt_count_iteration >=(work_part_from - 1) && pkt_count <= (work_part_to - 1))
                    {
                        pkt_offset = (pkt_count)* CurrParam.pkt_struct_size[CurrParam.current_file_index];
                        switch(db_codelist[CurrParam.current_file_index]){
                        case OBJ_MASTERBOARD:
                        {
                            data[pkt_offset + MBPKT_PARENT_ID_H] = (tbl_ParentDevices[pos].parent_id) >> 8;
                            data[pkt_offset + MBPKT_PARENT_ID_L] = (tbl_ParentDevices[pos].parent_id);
                            data[pkt_offset + MBPKT_ID_H] = (tbl_ParentDevices[pos].id) >> 8;
                            data[pkt_offset + MBPKT_ID_L] = (tbl_ParentDevices[pos].id);
                            data[pkt_offset + MBPKT_UIN_H] = (tbl_ParentDevices[pos].uin) >> 8;
                            data[pkt_offset + MBPKT_UIN_L] = (tbl_ParentDevices[pos].uin);

                            Ql_strcpy(&data[pkt_offset + MBPKT_ALIAS_H], tbl_ParentDevices[pos].d.etr.alias);

                            break;
                        }
                        case OBJ_ETR:
                        {
                            data[pkt_offset + ETRPKT_PARENT_ID_H] = (tbl_ParentDevices[pos].parent_id) >> 8;
                            data[pkt_offset + ETRPKT_PARENT_ID_L] = (tbl_ParentDevices[pos].parent_id);
                            data[pkt_offset + ETRPKT_ID_H] = (tbl_ParentDevices[pos].id) >> 8;
                            data[pkt_offset + ETRPKT_ID_L] = (tbl_ParentDevices[pos].id);
                            data[pkt_offset + ETRPKT_UIN_H] = (tbl_ParentDevices[pos].uin) >> 8;
                            data[pkt_offset + ETRPKT_UIN_L] = (tbl_ParentDevices[pos].uin);

                            Ql_strcpy(&data[pkt_offset + ETRPKT_ALIAS_H], tbl_ParentDevices[pos].d.etr.alias);
                            data[pkt_offset + ETRPKT_ETR_TYPE] = (tbl_ParentDevices[pos].d.etr.type);

                            data[pkt_offset + ETRPKT_RELATED_GROUPS_QTY_H] = (tbl_ParentDevices[pos].d.etr.related_groups_qty) >> 8;
                            data[pkt_offset + ETRPKT_RELATED_GROUPS_QTY_L] = (tbl_ParentDevices[pos].d.etr.related_groups_qty);

                            break;
                        }

                        case OBJ_RZE:                     
                        case OBJ_RAE:
                        {
                            data[pkt_offset + EPKT_PARENT_ID_H] = (tbl_ParentDevices[pos].parent_id) >> 8;
                            data[pkt_offset + EPKT_PARENT_ID_L] = (tbl_ParentDevices[pos].parent_id);
                            data[pkt_offset + EPKT_ID_H] = (tbl_ParentDevices[pos].id) >> 8;
                            data[pkt_offset + EPKT_ID_L] = (tbl_ParentDevices[pos].id);
                            data[pkt_offset + EPKT_UIN_H] = (tbl_ParentDevices[pos].uin) >> 8;
                            data[pkt_offset + EPKT_UIN_L] = (tbl_ParentDevices[pos].uin);
                            data[pkt_offset + EPKT_TYPE] = (tbl_ParentDevices[pos].type);

                            Ql_strcpy(&data[pkt_offset + EPKT_ALIAS_H], tbl_ParentDevices[pos].d.etr.alias);

                            break;
                        }
                        }
                        pkt_count++;
                    }
                    pkt_count_iteration++;
                }
            }

        }
        else
        {
            pkt_count = 0;

            for(u16 i = 0; i < items_in_file; ++i)
            {
            //
                db_offset = i * CurrParam.db_struct_size[CurrParam.current_file_index];
                pkt_offset = pkt_count * CurrParam.pkt_struct_size[CurrParam.current_file_index];

                // if this unit is included in the range of - using its
                if(i >=(work_part_from - 1) && i <= (work_part_to - 1))
                {

                    switch(db_codelist[CurrParam.current_file_index]){
                    case OBJ_ARMING_GROUP:
                    {
                        ArmingGroup *temp = (ArmingGroup *)&read_data[db_offset];
                        data[pkt_offset + AGPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + AGPKT_ID_L] = (temp->id);
                        Ql_strcpy(&data[pkt_offset + AGPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + AGPKT_RELATED_ETRs_QTY_H] = (temp->related_ETRs_qty) >> 8;
                        data[pkt_offset + AGPKT_RELATED_ETRs_QTY_L] = (temp->related_ETRs_qty);

                        break;
                    }
                    case OBJ_TOUCHMEMORY_CODE:
                    {
                        TouchmemoryCode *temp = (TouchmemoryCode *)&read_data[db_offset];

                        data[pkt_offset + TMCPKT_ID_L] = (temp->id) >> 8;
                        data[pkt_offset + TMCPKT_ID_L] = (temp->id);

                        Ql_memcpy(&data[pkt_offset + TMCPKT_KEY_H], temp->value, TOUCHMEMORY_MAX_KEY_SIZE);

                        data[pkt_offset + TMCPKT_KEY_SIZE] = temp->len;

                        Ql_strcpy(&data[pkt_offset + TMCPKT_ALIAS_H], temp->alias);

                        data[pkt_offset + TMCPKT_ACTION] = temp->action;

                        data[pkt_offset + TMCPKT_ARMING_GROUP_ID_H] = (temp->group_id) >> 8;
                        data[pkt_offset + TMCPKT_ARMING_GROUP_ID_L] = (temp->group_id);

                        break;
                    }
                    case OBJ_KEYBOARD_CODE:
                    {
                        KeyboardCode *temp = (KeyboardCode *)&read_data[db_offset];

                        data[pkt_offset + KBDCPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + KBDCPKT_ID_L] = (temp->id);
                        Ql_memcpy(&data[pkt_offset + KBDCPKT_KEY_H], temp->value, KEYBOARD_MAX_KEY_SIZE);
                        data[pkt_offset + KBDCPKT_KEY_SIZE] = temp->len;
                        Ql_strcpy(&data[pkt_offset + KBDCPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + KBDCPKT_ACTION] = temp->action;

                        data[pkt_offset + KBDCPKT_ARMING_GROUP_ID_H] = (temp->group_id) >> 8;
                        data[pkt_offset + KBDCPKT_ARMING_GROUP_ID_L] = (temp->group_id);
                        break;
                    }
                    case OBJ_ZONE:
                    {
                        Zone *temp = (Zone *)&read_data[db_offset];

                        data[pkt_offset + ZPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + ZPKT_ID_L] = (temp->id);
                        data[pkt_offset + ZPKT_HUMANIZED_ID_H] = (temp->humanized_id) >> 8;
                        data[pkt_offset + ZPKT_HUMANIZED_ID_L] = (temp->humanized_id);

                        Ql_strcpy(&data[pkt_offset + ZPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + ZPKT_SUIN] = temp->suin;
                        data[pkt_offset + ZPKT_ENABLE_FLAG] = temp->enabled;
                        data[pkt_offset + ZPKT_ZONE_TYPE] = temp->zone_type;

                        data[pkt_offset + ZPKT_ARMING_GROUP_ID_H] = (temp->group_id) >> 8;
                        data[pkt_offset + ZPKT_ARMING_GROUP_ID_L] = (temp->group_id);

                        data[pkt_offset + ZPKT_PARENTDEV_UIN_H] = (temp->puin) >> 8;
                        data[pkt_offset + ZPKT_PARENTDEV_UIN_L] = (temp->puin);
                        data[pkt_offset + ZPKT_PARENTDEV_TYPE] = temp->ptype;
                        data[pkt_offset + ZPKT_LOOP_DAMAGE_NOTIFICATION_FLAG] = temp->damage_notification_enabled;

                        break;
                    }
                    case OBJ_RELAY:
                    {
                        Relay *temp = (Relay *)&read_data[db_offset];

                        data[pkt_offset + RPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + RPKT_ID_L] = (temp->id);
                        data[pkt_offset + RPKT_HUMANIZED_ID_H] = (temp->humanized_id) >> 8;
                        data[pkt_offset + RPKT_HUMANIZED_ID_L] = (temp->humanized_id);

                        Ql_strcpy(&data[pkt_offset + RPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + RPKT_SUIN] = temp->suin;
                        data[pkt_offset + RPKT_ENABLE_FLAG] = temp->enabled;
                        data[pkt_offset + RPKT_REMOTE_CONTROL_FLAG] = temp->remote_control;
                        data[pkt_offset + RPKT_NOTIFY_ON_STATE_CHANGED_FLAG] = temp->notify_on_state_changed;
                        data[pkt_offset + RPKT_PARENTDEV_UIN_H] = (temp->puin) >> 8;
                        data[pkt_offset + RPKT_PARENTDEV_UIN_L] = (temp->puin);
                        data[pkt_offset + RPKT_PARENTDEV_TYPE] = temp->ptype;
                        data[pkt_offset + RPKT_LOAD_TYPE] = temp->load_type;

                        data[pkt_offset + RPKT_GROUP_ID_H] = (temp->group_id) >> 8;
                        data[pkt_offset + RPKT_GROUP_ID_L] = (temp->group_id);

                        break;
                    }
                    case OBJ_LED:
                    {
                        Led *temp = (Led *)&read_data[db_offset];

                        data[pkt_offset + LEDPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + LEDPKT_ID_L] = (temp->id);
                        data[pkt_offset + LEDPKT_HUMANIZED_ID_H] = (temp->humanized_id) >> 8;
                        data[pkt_offset + LEDPKT_HUMANIZED_ID_L] = (temp->humanized_id);

                        Ql_strcpy(&data[pkt_offset + LEDPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + LEDPKT_SUIN] = temp->suin;
                        data[pkt_offset + LEDPKT_ENABLE_FLAG] = temp->enabled;
                        data[pkt_offset + LEDPKT_PARENTDEV_UIN_H] = (temp->puin) >> 8;
                        data[pkt_offset + LEDPKT_PARENTDEV_UIN_L] = (temp->puin);
                        data[pkt_offset + LEDPKT_PARENTDEV_TYPE] = temp->ptype;
                        data[pkt_offset + LEDPKT_ARMING_LED_FLAG] = temp->b_arming_led;

                        data[pkt_offset + LEDPKT_GROUP_ID_H] = (temp->group_id) >> 8;
                        data[pkt_offset + LEDPKT_GROUP_ID_L] = (temp->group_id);

                        break;
                    }
                    case OBJ_BELL:
                    {
                        Bell *temp = (Bell *)&read_data[db_offset];

                        data[pkt_offset + BELLPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + BELLPKT_ID_L] = (temp->id);
                        data[pkt_offset + BELLPKT_HUMANIZED_ID_H] = (temp->humanized_id) >> 8;
                        data[pkt_offset + BELLPKT_HUMANIZED_ID_L] = (temp->humanized_id);

                        Ql_strcpy(&data[pkt_offset + BELLPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + BELLPKT_SUIN] = temp->suin;
                        data[pkt_offset + BELLPKT_ENABLE_FLAG] = temp->enabled;
                        data[pkt_offset + BELLPKT_REMOTE_CONTROL_FLAG] = temp->remote_control;

                        data[pkt_offset + BELLPKT_PARENTDEV_UIN_H] = (temp->puin) >> 8;
                        data[pkt_offset + BELLPKT_PARENTDEV_UIN_L] = (temp->puin);
                        data[pkt_offset + BELLPKT_PARENTDEV_TYPE] = temp->ptype;

                        break;
                    }

                    case OBJ_BUTTON:
                    {
                        Button *temp = (Button *)&read_data[db_offset];

                        data[pkt_offset + BTNPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + BTNPKT_ID_L] = (temp->id);
                        data[pkt_offset + BTNPKT_HUMANIZED_ID_H] = (temp->humanized_id) >> 8;
                        data[pkt_offset + BTNPKT_HUMANIZED_ID_L] = (temp->humanized_id);

                        Ql_strcpy(&data[pkt_offset + BTNPKT_ALIAS_H], temp->alias);
                        data[pkt_offset + BTNPKT_SUIN] = temp->suin;
                        data[pkt_offset + BTNPKT_ENABLE_FLAG] = temp->enabled;

                        data[pkt_offset + BTNPKT_PARENTDEV_UIN_H] = (temp->puin) >> 8;
                        data[pkt_offset + BTNPKT_PARENTDEV_UIN_L] = (temp->puin);
                        data[pkt_offset + BTNPKT_PARENTDEV_TYPE] = temp->ptype;

                        break;
                    }

                    case OBJ_COMMON_SETTINGS:
                    {
                        CommonSettings *temp = (CommonSettings *)&read_data[db_offset];

                        data[pkt_offset + CSPKT_CODEC_TYPE] = temp->codec_type;
                        data[pkt_offset + CSPKT_ENTRY_DELAY_H] = (temp->entry_delay_sec) >> 8;
                        data[pkt_offset + CSPKT_ENTRY_DELAY_L] = (temp->entry_delay_sec);
                        data[pkt_offset + CSPKT_ARMING_DELAY_H] = (temp->arming_delay_sec) >> 8;
                        data[pkt_offset + CSPKT_ARMING_DELAY_L] = (temp->arming_delay_sec);
                        data[pkt_offset + CSPKT_DEBUG_LEVEL] = temp->debug_level;
                        data[pkt_offset + CSPKT_AUX_PHONELIST_DEFAULT_SIZE] = temp->aux_phohelist_default_size;

                        data[pkt_offset + CSPKT_DTMF_PULSE_MS_H] = (temp->dtmf_pulse_ms) >> 8;
                        data[pkt_offset + CSPKT_DTMF_PULSE_MS_L] = (temp->dtmf_pulse_ms);
                        data[pkt_offset + CSPKT_DTMF_PAUSE_MS_H] = (temp->dtmf_pause_ms) >> 8;
                        data[pkt_offset + CSPKT_DTMF_PAUSE_MS_L] = (temp->dtmf_pause_ms);

                        u16 adc_m;
                        u16 adc_d;

                        adc_m = (int)temp->adc_a;
                        adc_d = (int)((temp->adc_a - adc_m)  * CSPKT_ADC_Precision);

                        data[pkt_offset + CSPKT_ADC_A_MANTISSA_H] = (adc_m) >> 8;
                        data[pkt_offset + CSPKT_ADC_A_MANTISSA_L] = (adc_m);
                        data[pkt_offset + CSPKT_ADC_A_DECIMAL_H] = (adc_d) >> 8;
                        data[pkt_offset + CSPKT_ADC_A_DECIMAL_L] = (adc_d);

                        adc_m = (int)temp->adc_b;
                        adc_d = (int)((temp->adc_b - adc_m)  * CSPKT_ADC_Precision);


                        data[pkt_offset + CSPKT_ADC_B_MANTISSA_H] = (adc_m) >> 8;
                        data[pkt_offset + CSPKT_ADC_B_MANTISSA_L] = (adc_m);
                        data[pkt_offset + CSPKT_ADC_B_DECIMAL_H] = (adc_d) >> 8;
                        data[pkt_offset + CSPKT_ADC_B_DECIMAL_L] = (adc_d);

                        break;
                    }
                    case OBJ_SYSTEM_INFO:
                    {
                        SystemInfo *temp = (SystemInfo *)&read_data[db_offset];

                        data[pkt_offset + SINFPKT_SETTINGS_SIGNATURE_3] = (temp->settings_signature) >> 24;
                        data[pkt_offset + SINFPKT_SETTINGS_SIGNATURE_2] = (temp->settings_signature) >> 16;
                        data[pkt_offset + SINFPKT_SETTINGS_SIGNATURE_1] = (temp->settings_signature) >> 8;
                        data[pkt_offset + SINFPKT_SETTINGS_SIGNATURE_0] = (temp->settings_signature);

                        Ql_memcpy(&data[pkt_offset + SINFPKT_SETTINGS_IDENT_STR_H], temp->db_structure_version, SYSTEM_INFO_STRINGS_LEN);

                        break;
                    }

                    case OBJ_IPLIST_SIM1:
                    case OBJ_IPLIST_SIM2:
                    {
                        IpAddress *temp = (IpAddress *)&read_data[db_offset];

                        data[pkt_offset + IPAPKT_ID] = temp->id;
                        data[pkt_offset + IPAPKT_IPADDRESS_0] = temp->address[0];
                        data[pkt_offset + IPAPKT_IPADDRESS_1] = temp->address[1];
                        data[pkt_offset + IPAPKT_IPADDRESS_2] = temp->address[2];
                        data[pkt_offset + IPAPKT_IPADDRESS_3] = temp->address[3];

                        break;
                    }

                    case OBJ_PHONELIST_SIM1:
                    case OBJ_PHONELIST_SIM2:
                    case OBJ_PHONELIST_AUX:
                    {
                        PhoneNumber *temp = (PhoneNumber *)&read_data[db_offset];

                        data[pkt_offset + PHNPKT_ID] = temp->id;
                        Ql_memcpy(&data[pkt_offset + PHNPKT_PHONENUMBER_H], temp->number, PHONE_LEN);
                        data[pkt_offset + PHNPKT_ALLOWED_SIMCARD] = temp->allowed_simcard;


                        break;
                    }

                    case OBJ_SIM_CARD:
                    {
                        SimSettings *temp = (SimSettings *)&read_data[db_offset];

                        data[pkt_offset + SIMPKT_ID] = temp->id;

                        data[pkt_offset + SIMPKT_USABLE] = temp->can_be_used;
                        data[pkt_offset + SIMPKT_PREFER_GPRS] = temp->prefer_gprs;
                        data[pkt_offset + SIMPKT_ALLOW_SMS] = temp->allow_sms_to_pult;

                        data[pkt_offset + SIMPKT_GPRS_ATTEMPTS_QTY] = temp->gprs_attempts_qty;
                        data[pkt_offset + SIMPKT_VOICECALL_ATTEMPTS_QTY] = temp->voicecall_attempts_qty;
        //                data[pkt_offset + SIMPKT_UDP_IPLIST_DEFAULT_SIZE] = temp->udp_ip_list;
        //                data[pkt_offset + SIMPKT_TCP_IPLIST_DEFAULT_SIZE] = temp->tcp_ip_list;
        //                data[pkt_offset + SIMPKT_PHONELIST_DEFAULT_SIZE] = temp->phone_list;

                        data[pkt_offset + SIMPKT_UDP_DESTPORT_H] = (temp->udp_ip_list.dest_port) >> 8;
                        data[pkt_offset + SIMPKT_UDP_DESTPORT_L] = (temp->udp_ip_list.dest_port);
                        data[pkt_offset + SIMPKT_UDP_LOCALPORT_H] = (temp->udp_ip_list.local_port) >> 8;
                        data[pkt_offset + SIMPKT_UDP_LOCALPORT_L] = (temp->udp_ip_list.local_port);

                        data[pkt_offset + SIMPKT_TCP_DESTPORT_H] = (temp->tcp_ip_list.dest_port) >> 8;
                        data[pkt_offset + SIMPKT_TCP_DESTPORT_L] = (temp->tcp_ip_list.dest_port);
                        data[pkt_offset + SIMPKT_TCP_LOCALPORT_H] = (temp->tcp_ip_list.local_port) >> 8;
                        data[pkt_offset + SIMPKT_TCP_LOCALPORT_L] = (temp->tcp_ip_list.local_port);

                        Ql_memcpy(&data[pkt_offset + SIMPKT_APN_H], temp->apn, APN_LEN);
                        Ql_memcpy(&data[pkt_offset + SIMPKT_LOGIN_H], temp->login, CREDENTIALS_LEN);
                        Ql_memcpy(&data[pkt_offset + SIMPKT_PASS_H], temp->password, CREDENTIALS_LEN);

                        break;
                    }

                    case OBJ_EVENT:
                    {
                        Event *temp = (Event *)&read_data[db_offset];


                        data[pkt_offset + EVTPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + EVTPKT_ID_L] = (temp->id);

                        Ql_strcpy(&data[pkt_offset + EVTPKT_ALIAS_H], temp->alias);

                        data[pkt_offset + EVTPKT_EVENT] = (temp->event_code);
                        data[pkt_offset + EVTPKT_EMITTER_TYPE] = (temp->emitter_type);

                        data[pkt_offset + EVTPKT_EMITTER_ID_H] = (temp->emitter_id) >> 8;
                        data[pkt_offset + EVTPKT_EMITTER_ID_L] = (temp->emitter_id);

                        data[pkt_offset + EVTPKT_SUBSCRIBERS_QTY] = (temp->subscribers_qty);
                        break;
                    }

                    case OBJ_BEHAVIOR_PRESET:
                    {
                        BehaviorPreset *temp = (BehaviorPreset *)&read_data[db_offset];


                        data[pkt_offset + BHPPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + BHPPKT_ID_L] = (temp->id);

                        Ql_strcpy(&data[pkt_offset + BHPPKT_ALIAS_H], temp->alias);

                        data[pkt_offset + BHPPKT_PULSES_IN_BATCH] = temp->pulses_in_batch;
                        data[pkt_offset + BHPPKT_PULSE_LEN] = temp->pulse_len;
                        data[pkt_offset + BHPPKT_PAUSE_LEN] = temp->pause_len;
                        data[pkt_offset + BHPPKT_BATCH_PAUSE_LEN] = temp->batch_pause_len;
                        data[pkt_offset + BHPPKT_BEHAVIOR_TYPE] = temp->behavior_type;

                        break;
                    }

                    case OBJ_REACTION:
                    {
                        Reaction *temp = (Reaction *)&read_data[db_offset];


                        data[pkt_offset + RCNPKT_ID_H] = (temp->id) >> 8;
                        data[pkt_offset + RCNPKT_ID_L] = (temp->id);

                        Ql_strcpy(&data[pkt_offset + RCNPKT_ALIAS_H], temp->alias);

                        data[pkt_offset + RCNPKT_EVENT_ID_H] = (temp->event_id) >> 8;
                        data[pkt_offset + RCNPKT_EVENT_ID_L] = (temp->event_id);
                        data[pkt_offset + RCNPKT_PERFORMER_TYPE] = temp->performer_type;
                        data[pkt_offset + RCNPKT_PERFORMER_ID_H] = (temp->performer_id) >> 8;
                        data[pkt_offset + RCNPKT_PERFORMER_ID_L] = (temp->performer_id);
                        data[pkt_offset + RCNPKT_PERFORMER_BEHAVIOR] = temp->performer_behavior;
                        data[pkt_offset + RCNPKT_BEHAVIOR_PRESET_ID_H] = (temp->behavior_preset_id) >> 8;
                        data[pkt_offset + RCNPKT_BEHAVIOR_PRESET_ID_L] = (temp->behavior_preset_id);
                        Ql_strcpy(&data[pkt_offset + RCNPKT_VALID_STATES_H], temp->valid_states);
                        data[pkt_offset + RCNPKT_REVERSIBLE_FLAG] = temp->bReversible;
                        data[pkt_offset + RCNPKT_REVERSE_PERFORMER_BEHAVIOR] = temp->reverse_behavior;
                        data[pkt_offset + RCNPKT_REVERSE_BEHAVIOR_PRESET_ID_H] = (temp->reverse_preset_id) >> 8;
                        data[pkt_offset + RCNPKT_REVERSE_BEHAVIOR_PRESET_ID_L] = (temp->reverse_preset_id);
                        break;
                    }

                    case OBJ_RELATION_ETR_AGROUP:
                    {
                        RelationETRArmingGroup *temp = (RelationETRArmingGroup *)&read_data[db_offset];

                        data[pkt_offset + REAGPKT_ETR_ID_H] = (temp->ETR_id) >> 8;
                        data[pkt_offset + REAGPKT_ETR_ID_L] = (temp->ETR_id);
                        data[pkt_offset + REAGPKT_GROUP_ID_H] = (temp->group_id) >> 8;
                        data[pkt_offset + REAGPKT_GROUP_ID_L] = (temp->group_id);

                        break;
                    }



                            default:
                            {
                                Ql_MEM_Free(read_data);
                                OUT_DEBUG_1("processDownloadingSettingsToPCSingle(): end file %s",
                                            db_filelist[CurrParam.current_file_index]);
                                fillTxBuffer_configurator(PC_DOWNLOAD_SETTINGS_FAILED, FALSE, NULL, 0);
                                sendBufferedPacket_configurator();
                                return ERR_READ_DB_FILE_FAILED;
                                break;
                            }
                            };

                    pkt_count++;

                }

            }

        }

    // close file
        Ql_MEM_Free(read_data);
        if(pkt_count > 0)
        {
            OUT_DEBUG_2("processDownloadingSettingsToPCSingle() SEND pkt_offset=%d, pkt_struct_size=%d \r\n" , pkt_offset, CurrParam.pkt_struct_size[CurrParam.current_file_index] );
            OUT_DEBUG_2("items_in_file=%d, work_part_from=%d, work_part_to=%d \r\n" ,items_in_file ,  work_part_from, work_part_to );

            fillTxBuffer_configurator(db_codelist[CurrParam.current_file_index], bAppend, data,  pkt_count * CurrParam.pkt_struct_size[CurrParam.current_file_index] );  // next block of the SQLITE DB file
            sendBufferedPacket_configurator();
        }







    return RETURN_NO_ERRORS;
}




//
bool createTempDBSkeleton_flash( void )
{
    OUT_DEBUG_2("createTempDB_flash()\r\n");

    // create TEMP_DB directory
    if (! createDirRecursively(db_dir_temp)) {
        OUT_DEBUG_1("createDir(\"%s\") = FALSE\r\n", db_dir_temp);
        return FALSE;
    }

    // create files
    for (u8 i = 0; i < sizeof(db_temp_filelist)/sizeof(u8*); ++i) {
        s32 fh = Ql_FS_Open(db_temp_filelist[i], QL_FS_CREATE_ALWAYS);
        if (fh < QL_RET_OK) {
            OUT_DEBUG_1("Error %d occured while creating or opening file \"%s\".\r\n",
                        fh, db_temp_filelist[i]);
            deleteDir(db_dir_temp);         // remove garbage
            return FALSE;
        }
        Ql_FS_Close(fh);
        OUT_DEBUG_3("File \"%s\" created.\r\n", db_temp_filelist[i]);
    }

    // create AUTOSYNC_TEMP_DB directory
    if (! createDirRecursively(db_dir_autosync_temp)) {
        OUT_DEBUG_1("createDir(\"%s\") = FALSE\r\n", db_dir_autosync_temp);
        return FALSE;
    }

    // create autosync files
//    for (u8 i = 0; i < sizeof(db_autosync_temp_filelist)/sizeof(u8*); ++i) {
//        s32 fh = Ql_FS_Open(db_autosync_temp_filelist[i], QL_FS_CREATE_ALWAYS);
//        if (fh < QL_RET_OK) {
//            OUT_DEBUG_1("Error %d occured while creating or opening file \"%s\".\r\n",
//                        fh, db_autosync_temp_filelist[i]);
//            deleteDir(db_dir_autosync_temp);         // remove garbage
//            return FALSE;
//        }
//        Ql_FS_Close(fh);
//        OUT_DEBUG_3("File \"%s\" created.\r\n", db_autosync_temp_filelist[i]);
//    }

    return TRUE;
}

bool restoreBackupDB_flash(void)
{
    OUT_DEBUG_2("restoreBackupDB_flash()\r\n");
    return copyDir(db_dir_backup, db_dir_working, NULL, FALSE);
}

s32 _createWorkingDB_ram(void)
{
    OUT_DEBUG_2("_createWorkingDB_ram()\r\n");

    // -- create DB objects
    Ar_GroupState_initStates();

    Ar_System_writeSizeAllFiles(0);

    // if a table can't be created => all following tables won't be created too
    bool bOk = TRUE;
    s32  ret = 0;

    bOk = bOk && 0 == (ret = createRuntimeObject_SystemInfo());

    // the version must be checked to prevent incorrect DB creation
    bOk = bOk && checkDBStructureVersion();

    bOk = bOk && 0 == (ret = createRuntimeObject_CommonSettings());
    bOk = bOk && 0 == (ret = createRuntimeTable_ArmingGroups());
    bOk = bOk && 0 == (ret = createRuntimeTable_KeyboardCodes());
    bOk = bOk && 0 == (ret = createRuntimeTable_TouchmemoryCodes());
    bOk = bOk && 0 == (ret = createRuntimeTable_ParentDevices());
    bOk = bOk && 0 == (ret = createRuntimeTable_Zones());
    bOk = bOk && 0 == (ret = createRuntimeTable_Relays());
    bOk = bOk && 0 == (ret = createRuntimeTable_Leds());
    bOk = bOk && 0 == (ret = createRuntimeTable_Bells());
    bOk = bOk && 0 == (ret = createRuntimeTable_Buttons());
    bOk = bOk && 0 == (ret = createRuntimeTable_Events());
    bOk = bOk && 0 == (ret = createRuntimeTable_BehaviorPresets());
    bOk = bOk && 0 == (ret = createRuntimeTable_Reactions());
    bOk = bOk && 0 == (ret = createRuntimeTable_RelationsETRArmingGroup());
    bOk = bOk && 0 == (ret = createRuntimeObject_PultMessageBuilder());
    bOk = bOk && 0 == (ret = createRuntimeArray_SimSettings());
    bOk = bOk && 0 == (ret = createRuntimeArray_AuxPhones());
    bOk = bOk && 0 == (ret = createRuntimeArray_Phones(SIM_1));
    bOk = bOk && 0 == (ret = createRuntimeArray_Phones(SIM_2));
    bOk = bOk && 0 == (ret = createRuntimeArray_IPs(SIM_1));
    bOk = bOk && 0 == (ret = createRuntimeArray_IPs(SIM_2));

    if (! bOk) {
        OUT_DEBUG_1("Some of createRuntime*() functions returned %d error\r\n", ret);
        ret = _clearWorkingDB_ram();
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("_clearWorkingDB_ram() = %d error\r\n", ret);
        }
        return ERR_CANNOT_CREATE_RAM_DATABASE;
    }

    return RETURN_NO_ERRORS;
}

s32 _clearWorkingDB_ram(void)
{
    OUT_DEBUG_2("_clearWorkingDB_ram()\r\n");
    bool bOk = TRUE;
    s32 ret = 0;

    bOk = 0 == (ret = destroyRuntimeObject_SystemInfo()) && bOk;
    bOk = 0 == (ret = destroyRuntimeObject_CommonSettings()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_ArmingGroups()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_KeyboardCodes()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_TouchmemoryCodes()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_ParentDevices()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Zones()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Relays()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Leds()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Bells()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Buttons()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Events()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_BehaviorPresets()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_Reactions()) && bOk;
    bOk = 0 == (ret = destroyRuntimeTable_RelationsETRArmingGroup()) && bOk;
    bOk = 0 == (ret = destroyRuntimeObject_PultMessageBuilder()) && bOk;
    bOk = 0 == (ret = destroyRuntimeArray_SimSettings()) && bOk;
    bOk = 0 == (ret = destroyRuntimeArray_AuxPhones()) && bOk;
    bOk = 0 == (ret = destroyRuntimeArray_Phones(SIM_1)) && bOk;
    bOk = 0 == (ret = destroyRuntimeArray_Phones(SIM_2)) && bOk;
    bOk = 0 == (ret = destroyRuntimeArray_IPs(SIM_1)) && bOk;
    bOk = 0 == (ret = destroyRuntimeArray_IPs(SIM_2)) && bOk;

    if (! bOk) {
        OUT_DEBUG_1("Some of destroyRuntime*() functions returned %d error\r\n", ret);
        return ERR_CANNOT_CLEAR_RAM_DATABASE;
    }

    return RETURN_NO_ERRORS;
}

s32 prepareDBinRAM(void)
{
    OUT_DEBUG_2("prepareDBinRAM()\r\n");

    s32 ret = _createWorkingDB_ram();

    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("_createWorkingDB_ram() = %d error. The database will be restored from the backup.\r\n", ret);

        if ( restoreBackupDB_flash() ) {
            ret = _createWorkingDB_ram();
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("_createWorkingDB_ram() = %d error. The database hasn't been created. Try to reconfigure the device.\r\n", ret);
                return ret;
            }
            DBRestoreLedMessage();
        } else {
            OUT_DEBUG_1("restoreBackupDB_flash() FAILED. The database hasn't been restored. Try to reconfigure the device.\r\n");
            return ERR_CANNOT_RESTORE_DB_BACKUP;
        }
}

    return RETURN_NO_ERRORS;
}

s32 reloadDBinRAM(void)
{
    OUT_DEBUG_2("reloadDBinRAM()\r\n");

    s32 ret = _clearWorkingDB_ram();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("_clearWorkingDB_ram() = %d error\r\n", ret);
        return ret;
    }

    ret = prepareDBinRAM();
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("prepareDBinRAM() = %d error\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}

void rollbackConfiguring(void)
{
    OUT_DEBUG_1("DB configuring FAILED.\r\n");
    OUT_DEBUG_2("rollbackConfiguring()\r\n");

    processDownloadingSettingsToPC(TRUE); // reset

    if (! deleteDir(db_dir_temp))     // clear garbage
        OUT_DEBUG_1("deleteDir(\"%s\") = FALSE\r\n", db_dir_temp);

    Ar_Configurator_setConnectionMode(CCM_NotConnected);
    _bTempDbSaved = FALSE;
}

s32 actualizationSIMCardLists(DbObjectCode DataType)
{
    s32 ret = 0;

    // DESTROY
    switch (DataType) {
    case OBJ_PHONELIST_SIM1:
        ret = destroyRuntimeArray_Phones(SIM_1);
        break;
    case OBJ_PHONELIST_SIM2:
        ret = destroyRuntimeArray_Phones(SIM_2);
        break;
    case OBJ_IPLIST_SIM1:
        ret = destroyRuntimeArray_IPs(SIM_1);
        break;
    case OBJ_IPLIST_SIM2:
        ret = destroyRuntimeArray_IPs(SIM_2);
        break;
    default:
        break;
    }

    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("actualizationSIMCardLists() Some of destroyRuntime*() functions returned %d error\r\n", ret);
        return ERR_CANNOT_CLEAR_RAM_DATABASE;
    }



    // CREATE
    switch (DataType) {
    case OBJ_PHONELIST_SIM1:
        ret = createRuntimeArray_Phones(SIM_1);
        break;
    case OBJ_PHONELIST_SIM2:
        ret = createRuntimeArray_Phones(SIM_2);
        break;
    case OBJ_IPLIST_SIM1:
        ret = createRuntimeArray_IPs(SIM_1);
        break;
    case OBJ_IPLIST_SIM2:
        ret = createRuntimeArray_IPs(SIM_2);
        break;
    default:
        break;
    }

    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("actualizationSIMCardLists() Some of createRuntime*() functions returned %d error\r\n", ret);
        return ERR_CANNOT_CREATE_RAM_DATABASE;
    }

    return RETURN_NO_ERRORS;
}


void DBRestoreLedMessage(void)
{

    BehaviorPreset behavior = {0};
    behavior.pulse_len = 4;
    behavior.pause_len = 2;
    behavior.batch_pause_len = 0;
    behavior.pulses_in_batch = 5;


    Led led;
    led.ptype = OBJ_MASTERBOARD;
    led.puin = 0x00;
    led.suin = 0x01;

    s32 ret = setLedUnitState(&led, UnitStateMultivibrator,  &behavior);




}

s32  FOTA_Upgrade(void)
{
    OUT_DEBUG_2("FOTA_Upgrade()\r\n");

    static ST_FotaConfig    FotaConfig;
    static u8 g_AppBinFile[64]=DBFILE_FOTA(FILENAME_PATTERN_FOTA_DB_FILE); //File name in file system
    /*---------------------------------------------------*/
    Ql_memset((void *)(&FotaConfig), 0, sizeof(ST_FotaConfig)); //Do not enable  watch_dog
    FotaConfig.Q_gpio_pin1 = PINNAME_DTR;
    FotaConfig.Q_feed_interval1 = 100;
    FotaConfig.Q_gpio_pin2 = PINNAME_NETLIGHT;
    FotaConfig.Q_feed_interval2 = 500;
    /*---------------------------------------------------*/

    char file_buffer[FOTA_READ_SIZE];
    s32 ret;
    u32 filesize;
    int hFile=-1;
    u32 realLen,lenToRead;

    ret = Ql_FS_Check((u8*)g_AppBinFile);
    if ( QL_RET_OK == ret)
    {
        filesize = Ql_FS_GetSize((u8*)g_AppBinFile);
        if( filesize < QL_RET_OK )
        {
            return ERR_FILENAME_IS_EMPTY;
        }

        ret = Ql_FS_Open((u8*)g_AppBinFile, QL_FS_READ_WRITE | QL_FS_CREATE);
        if( ret < QL_RET_OK )
        {
            return ERR_READ_DB_FILE_FAILED;
        }

        hFile=ret;//Get file handle

        ret=Ql_FOTA_Init(&FotaConfig);
        if( ret < QL_RET_OK )
        {
            OUT_DEBUG_1("Init fail\r\n");
            return ERR_READ_DB_FILE_FAILED;
        }

        OUT_DEBUG_2("Init Ok\r\n");
        OUT_DEBUG_2("Fota filesize=%d \r\n", filesize);

        Ql_Sleep(100);
        OUT_DEBUG_2("Start FOTA_WriteData \r\n");
        while(filesize>0)
        {
            if (filesize <= FOTA_READ_SIZE)
            {
                lenToRead = filesize;
            } else {
                lenToRead = FOTA_READ_SIZE;
            }
            Ql_FS_Read(hFile, file_buffer, lenToRead, &realLen);
            ret = Ql_FOTA_WriteData(realLen,(s8*)file_buffer);
            if( ret < QL_RET_OK )
            {
                OUT_DEBUG_1("Ql_FOTA_WriteData fail (error %d)\r\n", ret);
                return ERR_WRITE_DB_FILE_FAILED;
            }
            filesize -= realLen;
            Ql_Sleep(10);
        }
        OUT_DEBUG_2("End FOTA_WriteData \r\n");
        Ql_FS_Close(hFile);

        OUT_DEBUG_2("Close FOTA file\r\n");

        ret = Ql_FOTA_Finish();
        if( ret < QL_RET_OK )
        {
            OUT_DEBUG_1("Ql_FOTA_Finish(error %d)\r\n", ret);
            return ERR_UNDEFINED_ERROR;
        }

        Ql_FS_Delete((u8*)g_AppBinFile);


        ret = Ql_FOTA_Update();
        if( ret < QL_RET_OK )
        {
            OUT_DEBUG_1("Ql_FOTA_Update (error %d)\r\n", ret);
            Ql_Sleep(3000);
            Ql_Reset(0);
            return ERR_UNDEFINED_ERROR;
        }

    }
    else
    {
        OUT_DEBUG_2("FOTA file not found\r\n");
        return ERR_FILENAME_IS_EMPTY;
    }



    return RETURN_NO_ERRORS;
}
