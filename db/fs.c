/*
 * fs_access.c
 *
 * Created: 09.07.2012 11:26:33
 *  Author: user
 */

#include "fs.h"

#include "ql_error.h"
#include "ql_trace.h"
#include "ql_stdlib.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "db/db_serv.h"

#include "core/system.h"

s32 readFromDbFile(char *fileName, void *outData, u16 len, s32 offset)
{
    OUT_DEBUG_2("readFromDbFile(\"%s\")\r\n", fileName);

	u32 read_bytes = 0;
	s32 ret = QL_RET_OK;


    s32 fh = Ql_FS_Open(fileName, QL_FS_READ_ONLY);
	if (QL_RET_OK > fh) {
		OUT_DEBUG_1("Error %d occured while opening file \"%s\".\r\n", fh, fileName);
		return fh;
	}

    if (offset) {
        ret = Ql_FS_Seek(fh, offset, QL_FS_FILE_BEGIN);
		if (ret < QL_RET_OK) {
            Ql_FS_Close(fh);
			OUT_DEBUG_1("Error %d occured while seeking file \"%s\".\r\n", ret, fileName);
			return ret;
		}
	}

    ret = Ql_FS_Read(fh, (u8 *)outData, len, &read_bytes);
	if (QL_RET_OK != ret) {
        Ql_FS_Close(fh);
		OUT_DEBUG_1("Error %d occured while reading file \"%s\".\r\n", ret, fileName);
		return ret;
	}

//	*(outData + read_bytes) = '\0';
    Ql_FS_Close(fh);
	OUT_DEBUG_3("%d bytes read from file \"%s\".\r\n", read_bytes, fileName);

    return read_bytes;
}

s32 saveToDbFile(char *fileName, void *inData, u16 len, bool bAppend)
{
    OUT_DEBUG_2("saveToDbFile(\"%s\")\r\n", fileName);

	u32 written_bytes = 0;


    s32 fh = Ql_FS_Open(fileName, QL_FS_READ_WRITE);
	if (QL_RET_OK > fh) {
		OUT_DEBUG_1("Error %d occured while opening file \"%s\".\r\n", fh, fileName);
		return fh;
	}

    s32 ret = 0;
    if (bAppend) {
        ret = Ql_FS_Seek(fh, 0, QL_FS_FILE_END);
        if (ret < QL_RET_OK) {
            Ql_FS_Close(fh);
            OUT_DEBUG_1("Error %d occured while seeking file \"%s\".\r\n", ret, fileName);
            return ret;
        }
    }

    ret = Ql_FS_Write(fh, (u8 *)inData, len, &written_bytes);
	if (QL_RET_OK > ret) {
        Ql_FS_Close(fh);
		OUT_DEBUG_1("Error %d occured while writting file \"%s\".\r\n", ret, fileName);
		return ret;
	}

    Ql_FS_Close(fh);
    OUT_DEBUG_3("%d bytes %s to file \"%s\".\r\n", written_bytes, bAppend ? "appended" : "written", fileName);
    return written_bytes;
}


//
s32 CREATE_TABLE_FROM_DB_FILE(char *db_filename, void **pBuffer, u32 *db_file_size)
{
//	if (*pBuffer != NULL) {
//        OUT_DEBUG_1("The table of \"%s\" type already exists in RAM.\r\n", db_filename);
//		return ERR_DB_TABLE_ALREADY_EXISTS_IN_RAM;
//	}

	u32 local_size = 0;         // will be used if "db_file_size" parameter is not specified (is NULL)
    u32 *size = db_file_size ? db_file_size : &local_size;

    s32 ret = Ql_FS_GetSize(db_filename);
    if (ret < 0) return ret;
    else *size = ret;

    if (*pBuffer == NULL)
        *pBuffer = Ql_MEM_Alloc(*size);   // in bytes

    if (*pBuffer == NULL) {
        if (*size != 0) {
            OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", *size);
            return ERR_GET_NEW_MEMORY_FAILED;
        } else {
            OUT_DEBUG_1("DB file \"%s\" is empty\r\n", db_filename);
            return ERR_DB_FILE_IS_EMPTY;
        }
    }

    OUT_DEBUG_2("CREATE_TABLE_FROM_DB_FILE(\"%s\") on address %p\r\n", db_filename, *pBuffer);

	ret = readFromDbFile(db_filename, *pBuffer, *size, 0);  // to read entire file
	if (ret < RETURN_NO_ERRORS) {
        Ql_MEM_Free(*pBuffer);
        *pBuffer = NULL;
		OUT_DEBUG_1("readFromDbFile() = %d error for \"%s\" file.\r\n", ret, db_filename);
		return ret;
	}

    Ar_System_writeSizeAllFiles((u32) ret + Ar_System_readSizeAllFiles());
	return RETURN_NO_ERRORS;
}

//
s32 CREATE_SINGLE_OBJECT_FROM_DB_FILE(char *db_filename, void **pObject, u32 object_size, u8 object_id)
{
//	if (*pObject != NULL) {
//        OUT_DEBUG_1("The object of \"%s\" type already exists in RAM.\r\n", db_filename);
//		return ERR_DB_TABLE_ALREADY_EXISTS_IN_RAM;
//	}

    if (*pObject == NULL)
        *pObject = Ql_MEM_Alloc(object_size);   // in bytes
    if (*pObject == NULL) {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", object_size);
		return ERR_GET_NEW_MEMORY_FAILED;
    }

    OUT_DEBUG_2("CREATE_SINGLE_OBJECT_FROM_DB_FILE(\"%s\") on address %p\r\n", db_filename, *pObject);

    s32 offset = object_size * (object_id);
	s32 ret = readFromDbFile(db_filename, *pObject, object_size, offset);  // to read with offset
    if (ret < RETURN_NO_ERRORS) {
        Ql_MEM_Free(*pObject);
        *pObject = NULL;
		OUT_DEBUG_1("readFromDbFile() = %d error for \"%s\" file.\r\n", ret, db_filename);
		return ret;
	}

	return RETURN_NO_ERRORS;
}

bool deleteDir(char *dirName)
{
    OUT_DEBUG_2("deleteDir(\"%s\")\r\n", dirName);

    s32 ret = Ql_FS_CheckDir(dirName);
    if (ret == QL_RET_ERR_FILENOTFOUND) {
        OUT_DEBUG_3("Directory \"%s\" not found. Normally exit.\r\n", dirName);
        return TRUE;
    } else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Error %d occured while checking directory \"%s\".\r\n", ret, dirName);
        return FALSE;
    } else if (ret == QL_RET_OK) {
        ret = Ql_FS_XDelete(dirName, QL_FS_FILE_TYPE|QL_FS_DIR_TYPE|QL_FS_RECURSIVE_TYPE);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Error %d occured while deleting directory \"%s\".\r\n", ret, dirName);
            return FALSE;
        }
    }

    return TRUE;
}

bool copyDir(char *source, char *target, char *backupDirName, bool bKillSource)
{
    OUT_DEBUG_2("copyDir()%s: source - \"%s\", target - \"%s\"\r\n", backupDirName ? " with BACKING UP" : "", source, target);

    // -- check source dir
    s32 ret = Ql_FS_CheckDir(source);
    if (ret == QL_RET_ERR_FILENOTFOUND) {
        OUT_DEBUG_1("Directory \"%s\" was not found.\r\n", source);
        return FALSE;
    } else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Error %d occured while checking directory \"%s\".\r\n", ret, source);
        return FALSE;
    }

    // -- check and remove target dir
    ret = Ql_FS_CheckDir(target);
    if (ret < QL_RET_OK && ret != QL_RET_ERR_FILENOTFOUND) {
        OUT_DEBUG_1("Error %d occured while checking directory \"%s\".\r\n", ret, target);
        return FALSE;
    } else if (ret == QL_RET_OK) {      // backup and then delete or just delete only if the directory is exist

        if (backupDirName != NULL) {
            // -- move target dir to BACKUP dir and then remove target dir
            ret = Ql_FS_XMove(target, backupDirName, QL_FS_MOVE_KILL|QL_FS_MOVE_OVERWRITE);
            if (QL_RET_OK > ret) {
                OUT_DEBUG_1("Error %d occured while backing up \"%s\" directory.\r\n", ret, target, backupDirName);
                return FALSE;
            }
        } else {
            // -- just remove TARGET dir
            ret = Ql_FS_XDelete(target, QL_FS_FILE_TYPE|QL_FS_DIR_TYPE|QL_FS_RECURSIVE_TYPE);
            if (ret < QL_RET_OK) {
                OUT_DEBUG_1("Error %d occured while deleting directory \"%s\".\r\n", ret, target);
                return FALSE;
            }
        }
    }

    // -- move source dir to target dir
    ret = Ql_FS_XMove(source, target, bKillSource ? QL_FS_MOVE_KILL|QL_FS_MOVE_OVERWRITE : QL_FS_MOVE_COPY|QL_FS_MOVE_OVERWRITE);
    if (QL_RET_OK > ret) {
        OUT_DEBUG_1("Error %d occured while moving \"%s\" to \"%s\".\r\n", ret, source, target);
        return FALSE;
    }

    return TRUE;
}

bool createDir(char *dirName)
{
    OUT_DEBUG_2("createDir(\"%s\")\r\n", dirName);

    if (!Ql_strlen(dirName)) {
        OUT_DEBUG_1("Directory name is empty\r\n");
        return FALSE;
    }

    s32 ret = Ql_FS_CheckDir(dirName);
    if (QL_RET_ERR_FILENOTFOUND == ret) {
        ret = Ql_FS_CreateDir(dirName);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Error %d occured while creating directory \"%s\".\r\n", ret, dirName);
            return FALSE;
        }
    } else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Error %d occured while checking directory \"%s\".\r\n", ret, dirName);
        return FALSE;
    } else if (QL_RET_OK == ret) {
        OUT_DEBUG_3("Directory \"%s\" already exists thereby its content will be re-writed.\r\n", dirName);

        ret = Ql_FS_XDelete(dirName, QL_FS_FILE_TYPE|QL_FS_DIR_TYPE|QL_FS_RECURSIVE_TYPE);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Error %d occured while deleting directory \"%s\".\r\n", ret, dirName);
            return FALSE;
        }

        ret = Ql_FS_CreateDir(dirName);
        if (ret < QL_RET_OK) {
            OUT_DEBUG_1("Error %d occured while creating directory \"%s\".\r\n", ret, dirName);
            return FALSE;
        }
    }

    return TRUE;
}

bool createDirRecursively(char *dirName)
{
    OUT_DEBUG_2("createDirRecursively(\"%s\")\r\n", dirName);

    if (!Ql_strlen(dirName)) {
        OUT_DEBUG_1("Directory name is empty\r\n");
        return FALSE;
    }

    // -- create dirs hierarchy
    char parted_dir_name[255] = {0};
    Ql_strcpy(parted_dir_name, dirName);

    s16 nPathParts = 0;
    char *temp_str = parted_dir_name;
    const char path_separator = '\\';

    do {
        temp_str = Ql_strchr(temp_str, path_separator);
        if (temp_str) *temp_str++ = 0;
        ++nPathParts;
    } while (temp_str);

    while (nPathParts) {
        if (nPathParts-- > 1) {  // if not last iteration
            s32 ret = Ql_FS_CheckDir(parted_dir_name);
            if (ret < QL_RET_OK && QL_RET_ERR_FILENOTFOUND != ret) // if an error occured
            {
                OUT_DEBUG_1("Error %d occured while checking directory \"%s\".\r\n",
                            ret, parted_dir_name);
                return FALSE;
            }
            else if (QL_RET_OK == ret) // if the dir was found
            {
                OUT_DEBUG_3("Directory \"%s\" already exists. Creation skipped\r\n", parted_dir_name);
                char *end = Ql_strchr(parted_dir_name, 0);  // find the end of the dir name's part
                *end = path_separator;
                continue;
            }
        }

        // -- create dest dir
        if (!createDir(parted_dir_name))
            return FALSE;

        char *end = Ql_strchr(parted_dir_name, 0);  // find the end of the dir name's part
        *end = path_separator;
    }

    return TRUE;
}

s32 prepareEmptyDbFileInAutosyncTemporaryDB(char *fileName)
{
    OUT_DEBUG_2("prepareEmptyDbFileInAutosyncTemporaryDB(\"%s\")\r\n", fileName);

    char db_filename[100] = DBFILE_AUTOSYNC_TEMP("");        \
    Ql_strcat(db_filename, fileName);

    s32 fh = Ql_FS_Open(db_filename, QL_FS_CREATE_ALWAYS);
    if (QL_RET_OK > fh) {
        OUT_DEBUG_1("Error %d occured while create file \"%s\".\r\n", fh, db_filename);
        return fh;
    }

    Ql_FS_Close(fh);
    return RETURN_NO_ERRORS;
}


s32 prepareEmptyDbFileInAutosyncWorkingDB(char *fileName)
{
    OUT_DEBUG_2("prepareEmptyDbFileInAutosyncWorkingDB(\"%s\")\r\n", fileName);

    char db_filename[100] = DBFILE_AUTOSYNC("");        \
    Ql_strcat(db_filename, fileName);

    s32 fh = Ql_FS_Open(db_filename, QL_FS_CREATE_ALWAYS);
    if (QL_RET_OK > fh) {
        OUT_DEBUG_1("Error %d occured while create file \"%s\".\r\n", fh, db_filename);
        return fh;
    }

    Ql_FS_Close(fh);
    return RETURN_NO_ERRORS;
}



bool copyFilleFromAutosyncTemporaryDBToWorkDB(char *source)
{
    OUT_DEBUG_2("copyFilleFromAutosyncTemporaryDBToWorkDB(): source - \"%s\"\"\r\n", source);

    char source_filename[30] = DBFILE_AUTOSYNC_TEMP("");        \
    Ql_strcat(source_filename, source);


    char target_filename[30] = DBFILE_AUTOSYNC("");        \
    Ql_strcat(target_filename, source);


    // -- check source file
    s32 ret = Ql_FS_Check(source_filename);
    if (ret == QL_RET_ERR_FILENOTFOUND) {
        OUT_DEBUG_1("File \"%s\" was not found.\r\n", source_filename);
        return FALSE;
    } else if (ret < QL_RET_OK) {
        OUT_DEBUG_1("Error %d occured while checking file \"%s\".\r\n", ret, source_filename);
        return FALSE;
    }

    // -- check and remove target dir
    ret = Ql_FS_CheckDir(DB_DIR_AUTOSYNC);
    if (ret < QL_RET_OK && ret != QL_RET_ERR_FILENOTFOUND) {
        OUT_DEBUG_1("Error %d occured while checking dir \"%s\".\r\n",ret,  DB_DIR_AUTOSYNC);
        return FALSE;
    }

    // -- move source file to target dir
    ret = Ql_FS_XMove(source_filename, target_filename, QL_FS_MOVE_KILL|QL_FS_MOVE_OVERWRITE);
    if (QL_RET_OK > ret) {
        OUT_DEBUG_1("Error %d occured while moving \"%s\" to \"%s\".\r\n", ret, source_filename, target_filename);
        return FALSE;
    }

    return TRUE;
}
