#ifndef DB_MACRO_H
#define DB_MACRO_H


#define SAVING_TEMP_DB_TABLE_BEGIN(db_object_type, db_object_size)   \
    u16 received_obj_size = db_object_size;      \
    u16 struct_amount = pPkt->datalen / received_obj_size;  \
    u8 *pData = pPkt->data;        \
    u32 size = struct_amount * sizeof(db_object_type);  \
    void *write_buffer = Ql_MEM_Alloc(size);            \
    if (! write_buffer) {                               \
        OUT_DEBUG_1("Failed to get %d bytes of the HEAP memory\r\n", size); \
        return ERR_GET_NEW_MEMORY_FAILED;               \
    }                                                   \
    db_object_type *b = write_buffer;                   \
    for (u16 i = 0; i < struct_amount; ++i) {

#define SAVING_TEMP_DB_TABLE_END(filenamePattern)     \
        ++b;                                   \
        pData += received_obj_size;            \
    }           \
    char db_filename[100] = DBFILE_TEMP("");        \
    Ql_strcat(db_filename, filenamePattern);    \
    s32 ret = saveToDbFile(db_filename, write_buffer, size, TRUE);   \
    Ql_MEM_Free(write_buffer);                                               \
    if (ret < RETURN_NO_ERRORS) {                                              \
        OUT_DEBUG_1("saveToDbFile(\"%s\") = %d error.\r\n", db_filename, ret);    \
        return ERR_WRITE_DB_FILE_FAILED;        \
    }

#define SAVING_AUTOSYNC_TEMP_DB_TABLE_BEGIN SAVING_TEMP_DB_TABLE_BEGIN
#define SAVING_AUTOSYNC_TEMP_DB_TABLE_END(filenamePattern)     \
        ++b;                           \
        pData += received_obj_size;    \
    }                                  \
    char db_filename[100] = DBFILE_AUTOSYNC_TEMP("");        \
    Ql_strcat(db_filename, filenamePattern);    \
    s32 ret = saveToDbFile(db_filename, write_buffer, size, TRUE); \
    Ql_MEM_Free(write_buffer);     \
    if (ret < RETURN_NO_ERRORS) {      \
        OUT_DEBUG_1("saveToDbFile(\"%s\") = %d error.\r\n", db_filename, ret);    \
        return ERR_WRITE_DB_FILE_FAILED;     \
    }

#define COPY_ALIAS(dst_var_alias, src_alias_pkt_H) \
    for (u8 i = 0; i <= ALIAS_LEN; ++i)    \
        b->dst_var_alias[i] = (i < ALIAS_LEN) ? pData[src_alias_pkt_H + i] : 0;

#define COPY_U16(h_byte, l_byte)  (pData[h_byte] << 8 | pData[l_byte])
#define COPY_U8(byte)             (pData[byte])
#define COPY_BOOL(byte)           ((bool)pData[byte])
#define COPY_TYPE(byte)           ((DbObjectCode)pData[byte])
#define COPY_TYPE_EX(type, byte)  ((type)pData[byte])




#define SAVING_FOTA_FILE_BEGIN(db_object_type, db_object_size)   \
    u16 received_obj_size = db_object_size;      \
    u16 struct_amount = pPkt->datalen / received_obj_size;  \
    u8 *pData = pPkt->data;        \
    u32 size = struct_amount * sizeof(db_object_type);  \
    void *write_buffer = Ql_MEM_Alloc(size);            \
    if (! write_buffer) {                               \
        OUT_DEBUG_1("Failed to get %d bytes of the HEAP memory\r\n", size); \
        return ERR_GET_NEW_MEMORY_FAILED;               \
    }                                                   \
    db_object_type *b = write_buffer;                   \
    for (u16 i = 0; i < struct_amount; ++i) {

#define SAVING_FOTA_FILE_END(filenamePattern)     \
        ++b;                                   \
        pData += received_obj_size;            \
    }           \
    char db_filename[100] = DBFILE_FOTA("");        \
    Ql_strcat(db_filename, filenamePattern);    \
    s32 ret = saveToDbFile(db_filename, write_buffer, size, TRUE);   \
    Ql_MEM_Free(write_buffer);                                               \
    if (ret < RETURN_NO_ERRORS) {                                              \
        OUT_DEBUG_1("saveToDbFile(\"%s\") = %d error.\r\n", db_filename, ret);    \
        return ERR_WRITE_DB_FILE_FAILED;        \
    }


#endif // DB_MACRO_H
