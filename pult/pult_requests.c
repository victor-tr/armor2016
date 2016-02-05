#include "pult_requests.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "pult/pult_tx_buffer.h"

#include "db/fs.h"

#include "service/crc.h"

#include "states/armingstates.h"

#include "core/system.h"

s32 rx_handler_requestReject(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_requestReject");


    return RETURN_NO_ERRORS;
}


s32 rx_handler_deviceInfo(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_deviceInfo\r\n");

    SystemInfo *pSysInfo = getSystemInfo(NULL);

    u8 data[5] = {1,
                  pSysInfo->firmware_version_major,
                  pSysInfo->firmware_version_minor,
                  0,
                  1};
    return reportToPultComplex(data, sizeof(data), PMC_Response_DeviceInfo, PMQ_AuxInfoMsg);
}


s32 rx_handler_connectionTest(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_connectionTest\r\n");

    u8 data[1] = {1};
    return reportToPultComplex(data, sizeof(data), PMC_ConnectionTest, PMQ_AuxInfoMsg);
}


s32 rx_handler_armedGroup(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_armedGroup\r\n");
    u8 data[2] = {0,
                 0};

    u16 groupPKT = pMsg->complex_msg_part[0];
    data[0] = groupPKT;

    ArmingGroup *pGroup = 0;
    pGroup = getArmingGroupByID(groupPKT);
    if(!pGroup){
        data[1] = 0;
        OUT_DEBUG_2("Group not found\r\n");
        return reportToPultComplex(data, sizeof(data), PMC_Request_ArmedGroup, PMQ_AuxInfoMsg);
    }
    else {
        data[1] = 1;
        if(TRUE == Ar_GroupState_isArmed(pGroup)){
            OUT_DEBUG_2("Group armed\r\n");
            return reportToPultComplex(data, sizeof(data), PMC_Request_ArmedGroup, PMQ_AuxInfoMsg);
        }
        else{
            OUT_DEBUG_2("Group disarmed\r\n");
            u16 id = pGroup->tbl_related_ETRs.t[0]->id;
            ParentDevice *pParent = getParentDeviceByID(id, OBJ_ETR);
            pGroup->change_status_ETR=pParent;


            startArmingDelay(pGroup);
            Ar_GroupState_setState(pGroup, STATE_ARMED);



            PultMessage msg = {0};


            msg.identifier = 0;
            msg.group_id = pGroup->id;

            msg.msg_code = PMC_OpenClose;
            msg.msg_qualifier = PMQ_RestoralOrSetArmed;

            s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
                return ret;
            }



            //
        }
    }


    return RETURN_NO_ERRORS;
}


s32 rx_handler_disarmedGroup(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_disarmedGroup");

    u8 data[2] = {0,
                 0};

    u16 groupPKT = pMsg->complex_msg_part[0];
    data[0] = groupPKT;

    ArmingGroup *pGroup = 0;
    pGroup = getArmingGroupByID(groupPKT);
    if(!pGroup){
        data[1] = 0;
        OUT_DEBUG_2("Group not found\r\n");
        return reportToPultComplex(data, sizeof(data), PMC_Request_DisarmedGroup, PMQ_AuxInfoMsg);
    }
    else {
        data[1] = 1;
        if(FALSE == Ar_GroupState_isArmed(pGroup)){
            OUT_DEBUG_2("Group disarmed\r\n");
            return reportToPultComplex(data, sizeof(data), PMC_Request_DisarmedGroup, PMQ_AuxInfoMsg);
        }
        else{
            OUT_DEBUG_2("Group armed\r\n");
            u16 id = pGroup->tbl_related_ETRs.t[0]->id;
            ParentDevice *pParent = getParentDeviceByID(id, OBJ_ETR);

            Ar_System_setPrevETRLed(pGroup, UnitStateOff);

            Ar_System_stopPrevETRBuzzer(pGroup);

            stopBothDelays(pGroup);

            Ar_GroupState_setState(pGroup, STATE_DISARMED);

            //==================
            Ar_System_setBellState(UnitStateOff);

            PultMessage msg = {0};


            msg.identifier = 0;
            msg.group_id = pGroup->id;

            msg.msg_code = PMC_OpenClose;
            msg.msg_qualifier = PMQ_NewMsgOrSetDisarmed;

            s32 ret = reportToPult(msg.identifier, msg.group_id, msg.msg_code, msg.msg_qualifier);
            if (ret < RETURN_NO_ERRORS) {
                OUT_DEBUG_1("reportToPult() = %d error\r\n", ret);
                return ret;
            }

            //
        }
    }


    return RETURN_NO_ERRORS;
}

s32 rx_handler_batteryVoltage(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_batteryVoltage");


    u8 data[4] = {0,
                  0,
                  0,
                  0};

    // -- save current voltage
    float current_voltage = Ar_System_getLastVoltage();
    u8 iVoltage = (u8)current_voltage;    // integer part of the float voltage
    u8 fVoltage = (u8)((current_voltage - (u8)current_voltage) * 10); // fractional part of the float voltage

    u8 iMax = 14;
    float fVoltageDiapazon[16] ={7,
                                 7.5,
                                 8,
                                 8.5,
                                 9,
                                 9.5,
                                10,
                                10.5,
                                11,
                                11.5,
                                12,
                                12.5,
                                13,
                                13.5};

    u8 aHour[16] = {0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    1,
                    2,
                    4,
                    6,
                    9,
                    12,
                    13,
                    14};

    u8 aMinute[16] = {0,
                    10,
                    20,
                    30,
                    40,
                    50,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0};


    u8 iHour = 0;
    u8 iMinute = 0;
    for(u8 i = 0; i < iMax; ++i){
        if(fVoltageDiapazon[i] < current_voltage){
            iHour = aHour[i];
            iMinute = aMinute[i];
        }
    }

    data[0] =  iVoltage;
    data[1] =  fVoltage;
    data[2] =  iHour;
    data[3] =  iMinute;
    return reportToPultComplex(data, sizeof(data), PMC_Response_BatteryVoltage, PMQ_AuxInfoMsg);

}


s32 rx_handler_deviceState(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_deviceState\r\n");

    SystemInfo *pSysInfo = getSystemInfo(NULL);
    u8 data[5] = {pSysInfo->settings_signature_autosync,
                  pSysInfo->settings_signature_autosync,
                  1};
    return reportToPultComplex(data, sizeof(data), PMC_Response_DeviceState, PMQ_AuxInfoMsg);
}


s32 rx_handler_loopsState(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_loopsState \r\n");

    Zone_tbl *pZones = getZoneTable(NULL);
    Zone *pZone = 0;
    ArmingGroup *pGroup = 0;
    u16 dataLen;
    u16 dataLen_1;
    u16 dataLen_2;
    u16 dataLen_3;

    u16 firstZonePKT = pMsg->complex_msg_part[0];
    u16 zoneCountPKT = pMsg->complex_msg_part[1];
    u16 detailLevel =  pMsg->complex_msg_part[2];

    // ----------------------------------------------------------------------
    u8 zoneEnabled;
    u8 zoneState;
    u8 zoneGroupID;
    u8 zoneArmed;
    u8 zoneType;
    u8 zoneLastEventType;

    OUT_DEBUG_7("firstZonePKT: %d, zoneCountPKT: %d, detailLevel: %d, pZones->size:  %d\r\n",
                firstZonePKT, zoneCountPKT, detailLevel, pZones->size);

    if(firstZonePKT == 0){
        firstZonePKT = 1;
        zoneCountPKT = pZones->size;
    }
    else {
        // first zone
        if(firstZonePKT > pZones->size)
            firstZonePKT = pZones->size;

        // zones count
        if(((firstZonePKT-1) + zoneCountPKT) > pZones->size)
            zoneCountPKT = pZones->size - (firstZonePKT - 1);
    }
    OUT_DEBUG_7("firstZonePKT: %d, zoneCountPKT: %d\r\n",
                firstZonePKT, zoneCountPKT);

    //data len
    dataLen_1 = zoneCountPKT / 8;
    if(zoneCountPKT % 8) dataLen_1++;

    dataLen_2 = zoneCountPKT / 2;
    if(zoneCountPKT % 2) dataLen_2++;

    dataLen_3 = zoneCountPKT * 2;


    OUT_DEBUG_7("finis -> dataLen_1: %d, dataLen_2: %d, dataLen_3: %d\r\n",
                    dataLen_1, dataLen_2, dataLen_3);

    // buffer size
    switch (detailLevel){
    case 1:
    case 49:
        dataLen = dataLen_1;
        detailLevel = 1;
        break;
    case 2:
    case 50:
        dataLen = dataLen_2;
        detailLevel = 2;
        break;
    case 3:
    case 51:
    default:
        dataLen = dataLen_3;
        detailLevel = 3;
        break;
    }
    //----------------------------------
    OUT_DEBUG_7("dataLen: %d, detailLevel: %d\r\n",
                dataLen,  detailLevel);

    // create buffer
    u8 *data = Ql_MEM_Alloc(dataLen + 3);
    if(!data)
    {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", dataLen + 3);
        u8 data_err[2] = {RCE_MemoryAllocationError,0};
        reportToPultComplex(data_err, 2, PMC_CommandReject, PMQ_AuxInfoMsg);
        return ERR_GET_NEW_MEMORY_FAILED;
    }
    else{//++++++++++++++++++++++++++++++++++++++++++++

    for(u8 j = 0; j < dataLen + 3; ++j){
        data[j] = 0;
    }

    data[0] = firstZonePKT;
    data[1] = zoneCountPKT;
    data[2] = detailLevel;


    u16 indexData = 0;
    u8 counterBit = 0; // from 0 to 7
    u8 zoneStateBuffer = 0;
    u8 zoneTetradaBuffer = 0;
    u8 rorTemplate = 0;
    u8 tempByte = 0;
    u8 triadaByte =0;
    u8 k;

    for (u16 i = (firstZonePKT - 1); i <= (zoneCountPKT + firstZonePKT - 2); ++i) {
        pZone = &pZones->t[i];

        zoneEnabled = (pZone->enabled == TRUE)? 1 : 0;
        zoneState =(pZone->zoneDamage_counter == 0)? 1 : 0;
        zoneGroupID = pZone->group_id;

        pGroup = getArmingGroupByID(pZone->group_id);
        if(!pGroup){
            zoneArmed = 0;
        }
        else {
            zoneArmed = (TRUE == Ar_GroupState_isArmed(pGroup))? 1 : 0;
        }

        zoneType = pZone->zone_type;
        zoneLastEventType = pZone->last_event_type;


        //fill buffer
        switch (detailLevel){
        case 1:
            // поставить zoneArmed
            // в data[indexData]
            // на место counterBit
            // все из i растет


            k = i - (firstZonePKT - 1);
            indexData = k / 8 + 3;
            counterBit = k - (k / 8) * 8;

//                OUT_DEBUG_7("i: %d, k: %d, indexData: %d , counterBit: %d \r\n",
//                            i, k, indexData, counterBit);

            tempByte = zoneState << counterBit;
            data[indexData] = (data[indexData] | tempByte);
            break;
        case 2:
            // в data[indexData]

            k = i - (firstZonePKT - 1);
            indexData = k / 2 + 3;

            //===================
            // формируем triadaByte
            triadaByte = 0;
            triadaByte = triadaByte | zoneEnabled;
            triadaByte = triadaByte << 1;
            triadaByte = triadaByte | zoneArmed;
            triadaByte = triadaByte << 2;
            switch (zoneLastEventType) {

            case ZONE_EVENT_BREAK:
                triadaByte = triadaByte | 1;
                break;
            case ZONE_EVENT_SHORT:
                triadaByte = triadaByte | 2;
                break;
            case ZONE_EVENT_FITTING:
                triadaByte = triadaByte | 3;
                break;
            case ZONE_EVENT_NORMAL:
            default:
                triadaByte = triadaByte | 0;

            }
            //===================




            if(k % 2) triadaByte = triadaByte << 4;

            data[indexData] = (data[indexData] | triadaByte);

            break;
        case 3:
        default:
            // в data[indexData]

            k = i - (firstZonePKT - 1);
            indexData = k * 2 + 3;


            data[indexData] = zoneType << 4;

            //===================
            // формируем triadaByte
            triadaByte = 0;
            triadaByte = triadaByte | zoneEnabled;
            triadaByte = triadaByte << 1;
            triadaByte = triadaByte | zoneArmed;
            triadaByte = triadaByte << 2;
            switch (zoneLastEventType) {

            case ZONE_EVENT_BREAK:
                triadaByte = triadaByte | 1;
                break;
            case ZONE_EVENT_SHORT:
                triadaByte = triadaByte | 2;
                break;
            case ZONE_EVENT_FITTING:
                triadaByte = triadaByte | 3;
                break;
            case ZONE_EVENT_NORMAL:
            default:
                triadaByte = triadaByte | 0;

            }
            //===================
            data[indexData] = data[indexData] | triadaByte;
            data[indexData + 1] = zoneGroupID;
            break;

        }



//            OUT_DEBUG_7("pZone[%d]->enabled: %d , state: %d , group_id: %d, zoneArmed: %d ,  zoneType: , %d  zoneLastEventType: %d \r\n",
//                        i, zoneEnabled, zoneState, zoneGroupID, zoneArmed, zoneType, zoneLastEventType);

    }

//    for (u16 ii = 0; ii <(dataLen + 3); ++ii) {
//        OUT_DEBUG_7("data[%d]: %d \r\n",
//                    ii, data[ii]);

//    }

    reportToPultComplex(data, dataLen + 3, PMC_Response_LoopsState, PMQ_AuxInfoMsg);

    Ql_MEM_Free(data);
    }//++++++++++++++++++++++++++++++++++++++++++
    //-----------------------------------------------------------------------



    return RETURN_NO_ERRORS;
}


s32 rx_handler_groupsState(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_groupsState\r\n");

    ArmingGroup_tbl *pGroups = getArmingGroupTable(NULL);
    ArmingGroup *pGroup = 0;

    u16 dataLen;
    u16 firstGroupPKT = pMsg->complex_msg_part[0];
    u16 groupCountPKT = pMsg->complex_msg_part[1];

    OUT_DEBUG_7("firstGroupPKT: %d, groupCountPKT: %d, pZones->size:  %d\r\n",
                firstGroupPKT, groupCountPKT, pGroups->size);

    if(firstGroupPKT == 0){
        firstGroupPKT = 1;
        groupCountPKT = pGroups->size;
    }
    else {
        // first zone
        if(firstGroupPKT > pGroups->size)
            firstGroupPKT = pGroups->size;

        // zones count
        if(((firstGroupPKT-1) + groupCountPKT) > pGroups->size)
            groupCountPKT = pGroups->size - (firstGroupPKT - 1);
    }
    OUT_DEBUG_7("firstGroupPKT: %d, groupCountPKT: %d\r\n",
                firstGroupPKT, groupCountPKT);


    // buffer size
    dataLen = groupCountPKT / 2;
    if(groupCountPKT % 2) dataLen++;

    // create buffer
    u8 *data = Ql_MEM_Alloc(dataLen + 2);
    if(!data)
    {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", dataLen + 2);
        u8 data_err[2] = {RCE_MemoryAllocationError,0};
        reportToPultComplex(data_err, 2, PMC_CommandReject, PMQ_AuxInfoMsg);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    for(u8 j = 0; j < dataLen + 2; ++j){
        data[j] = 0;
    }

    u16 indexData = 0;
    u8 tempData;
    u8 k;

    data[0] = firstGroupPKT;
    data[1] = groupCountPKT;
        for (u16 i = (firstGroupPKT - 1); i <= (groupCountPKT + firstGroupPKT - 2); ++i) {
            pGroup = &pGroups->t[i];
            k = i - (firstGroupPKT - 1);
            indexData = k / 2 + 2;

            // --- Set  bits
            // bit0: 1-Normal/0-Others
            // bit1: reserved
            // bit2: 1-Armrd /0-Others
            // bit3: 1-Used  /0-Others
            tempData = 0;


            if(TRUE == Ar_GroupState_isArmed(pGroup)){
                tempData  = tempData | 4;
            }

            if(TRUE == Ar_System_isZonesInGroupNormal(pGroup))
                tempData  = tempData | 1;
            // ---

            tempData  = tempData | 8;

            if(k % 2)
                tempData = tempData << 4;

            data[indexData] = data[indexData] | tempData;


        }

//        for (u16 ii = 0; ii <(dataLen + 2); ++ii) {
//            OUT_DEBUG_7("data[%d]: %d \r\n",
//                        ii, data[ii]);
//        }

        reportToPultComplex(data, sizeof(data), PMC_Response_GroupsState, PMQ_AuxInfoMsg);
        Ql_MEM_Free(data);
        return RETURN_NO_ERRORS;


}


s32 rx_handler_readTable(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_readTable\r\n");


    return RETURN_NO_ERRORS;
}


s32 rx_handler_saveTableSegment(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_saveTableSegment\r\n");
    OUT_DEBUG_2("pMsg->part_len = %d\r\n", pMsg->part_len);
    if(4 == pMsg->part_len){
        return RETURN_NO_ERRORS;
    }


    RxConfiguratorPkt pkt;

    pkt.dataType=pMsg->complex_msg_part[0];
    pkt.data=&pMsg->complex_msg_part[4];


    pkt.datalen= pMsg->part_len-4;

    pkt.marker=CONFIGURATION_AUTOSYNC_PACKET_MARKER;
    pkt.pktIdx=0;
    pkt.repeatCounter=0;
    pkt.sessionKey=0;
    pkt.bAppend=TRUE;


    char name[30] = {0};

    switch (pkt.dataType) {
    case OBJ_PHONELIST_SIM1:
        Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM1);
        break;
    case OBJ_PHONELIST_SIM2:
        Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM2);
        break;
    case OBJ_IPLIST_SIM1:
        Ql_strcpy(name, FILENAME_PATTERN_IPLIST_SIM1);
        break;
    case OBJ_IPLIST_SIM2:
        Ql_strcpy(name, FILENAME_PATTERN_IPLIST_SIM2);
        break;
    default:
        break;
    }

    //===================================
    // PREPARE Empty file
    if(0 == pMsg->complex_msg_part[1])
    {
        s32 ret = prepareEmptyDbFileInAutosyncTemporaryDB(name);
        if(ret < RETURN_NO_ERRORS)
        {
            OUT_DEBUG_1("prepareEmptyDbFileInAutosyncTemporaryDB() = %d error. File not create.\r\n", ret);
            return ret;
        }
    }

    //===================================
    // SAVE to AutosyncTemporaryDB
    s32 ret = saveSettingsToAutosyncTemporaryDB(&pkt);
    if(ret < RETURN_NO_ERRORS)
    {
        OUT_DEBUG_1("saveSettingsToAutosyncTemporaryDB() = %d error\r\n", ret);
        return ret;
    }


    //===================================
    // MOVE to work file
    if(pMsg->complex_msg_part[1] == pMsg->complex_msg_part[2])
    {
        if(!copyFilleFromAutosyncTemporaryDBToWorkDB(name))
        {
            OUT_DEBUG_1("copyFilleFromAutosyncTemporaryDBToWorkDB() = %d error. File not move.\r\n");
            return ERR_DB_SAVING_FAILED;
        }

        // ACTUALIZATION
        ret = actualizationSIMCardLists(pkt.dataType);
        if(ret < RETURN_NO_ERRORS)
        {
            OUT_DEBUG_1("actualizationSIMCardLists() = %d error. Actualize error.\r\n", ret);
            return ret;
        }

        SystemInfo *pSysInfo = getSystemInfo(NULL);
        pSysInfo->settings_signature_autosync = pMsg->complex_msg_part[3];

        // save to DB signature autosync
        ret = saveToDbFile(DBFILE(FILENAME_PATTERN_SYSTEMINFO), pSysInfo, sizeof(SystemInfo), FALSE);
        if(ret < RETURN_NO_ERRORS)
        {
            OUT_DEBUG_1("saveToDbFile() = %d error. Save signature autosynce error.\r\n", ret);
            return ret;
        }
        OUT_DEBUG_7("SystemInfo settings_signature_autosync = %d.\r\n", pSysInfo->settings_signature_autosync);
    }

    return RETURN_NO_ERRORS;
}


s32 rx_handler_requestVoiceCall(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_requestVoiceCall\r\n");


    return RETURN_NO_ERRORS;
}


s32 rx_handler_settingsData(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_settingsData\r\n");



    u8 *buf = pMsg->complex_msg_part;
    if(!checkConfiguratorCRC(buf))
    {
        OUT_DEBUG_1("rx_handler_settingsData() = CRC settingsPKT error.\r\n");
        return ERR_BAD_CRC;
    }



    RxConfiguratorPkt pkt;

    pkt.bAppend =       buf[SPS_APPEND_FLAG];
    pkt.data =         &buf[SPS_PREFIX];
    pkt.datalen =       buf[SPS_BYTES_QTY_H] << 8 | buf[SPS_BYTES_QTY_L];
    pkt.dataType =      (DbObjectCode)buf[SPS_FILECODE];
    pkt.marker =        buf[SPS_MARKER];
    pkt.pktIdx =        buf[SPS_PKT_INDEX];
    pkt.repeatCounter = buf[SPS_PKT_REPEAT];
    pkt.sessionKey =    buf[SPS_PKT_KEY];

    s32 ret = Ar_Configurator_processSettingsData(&pkt, CCM_RemotePultConnection);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("Ar_Configurator_processSettingsData() = %d error.\r\n", ret);
    }


    return RETURN_NO_ERRORS;
}
