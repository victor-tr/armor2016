/*
 * fs_access.h
 *
 * Created: 09.07.2012 11:27:10
 *  Author: user
 */
#ifndef FS_H
#define FS_H

#include "ql_type.h"
#include "ql_fs.h"
#include "db.h"


/************************************************************************
* Get all data from file to bytes array.
* Returns: actually read bytes. If returned value < 0 => see errors codes.
************************************************************************/
s32 readFromDbFile(char *fileName, void *outData, u16 len, s32 offset);

/************************************************************************
* Open the file and save data to it.
* Returns: actually written bytes. If returned value < 0 => see errors codes.
************************************************************************/
s32 saveToDbFile(char *fileName, void *inData, u16 len, bool bAppend);

//************************************
// Returns:   s32 error code
// Qualifier:
// Parameter: u8 * db_filename   - [IN]  DB file's name
// Parameter: void * pBuffer     - [IN]  pointer to appropriate table in RAM;
//                                 if (pBuffer == NULL) then new memory block of the required size will be allocated,
//                                 otherwise the *pBuffer address will be used
// Parameter: u32 * db_file_size - [OUT] pointer to variable that will receive the file size in bytes, can be NULL if don't needed
//************************************
s32 CREATE_TABLE_FROM_DB_FILE(char *db_filename, void **pBuffer, u32 *db_file_size);

//************************************
// Returns:   s32 error code
// Qualifier:
// Parameter: u8 * db_filename  - [IN] DB file's name
// Parameter: void * pObject    - [IN] pointer to appropriate object in RAM
//                                 if (pObject == NULL) then new memory block of the required size will be allocated,
//                                 otherwise the *pBuffer address will be used
// Parameter: u32 object_size   - [IN] object size in bytes
// Parameter: s32 object_id     - [IN] index of the object in table (from 0 to the table end)
//************************************
s32 CREATE_SINGLE_OBJECT_FROM_DB_FILE(char *db_filename, void **pObject, u32 object_size, u8 object_id);

//
bool deleteDir(char *dirName);

// if backing up isn't needed => backupDirName must be NULL
bool copyDir(char *source, char *target, char *backupDirName, bool bKillSource);

//
bool createDir(char *dirName);
bool createDirRecursively(char *dirName);


/************************************************************************
* Prepere empty file for autosync TEMP DIR.
* Returns: actually written bytes. If returned value < 0 => see errors codes.
************************************************************************/
s32 prepareEmptyDbFileInAutosyncTemporaryDB(char *fileName);
s32 prepareEmptyDbFileInAutosyncWorkingDB(char *fileName);

// copy file from AutosyncTemporary folder to Work folder
bool copyFilleFromAutosyncTemporaryDBToWorkDB(char *source);

#endif /* FS_H */
