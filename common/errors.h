/*
 * errors.h
 *
 * Created: 19.10.2012 12:05:11
 *  Author: user
 */


#ifndef ERRORS_H_
#define ERRORS_H_

#include "ql_error.h"


// -- errors
typedef enum _CustomerErrors {
    RETURN_NO_ERRORS                = 0,

    ERR_UNDEFINED_ERROR             = -19999,           // unknown error
// -- memory management errors
    ERR_GET_NEW_MEMORY_FAILED       = -20000,           // can't allocate required memory in the HEAP
// -- UART errors
    ERR_BAD_CRC                     = -20001,
    ERR_CANNOT_READ_RX_UART_DATA    = -20002,
    ERR_ZERO_LENGTH_DATA            = -20003,          // no data to send
// -- DB errors
    ERR_DB_SAVING_FAILED            = -20011,           //
    ERR_DB_SKELETON_NOT_CREATED     = -20012,           //
    ERR_WRITE_DB_FILE_FAILED        = -20013,           //
    ERR_READ_DB_FILE_FAILED         = -20014,           //
    ERR_DB_TABLE_ALREADY_EXISTS_IN_RAM     = -20015,    //
    ERR_DB_FILE_IS_EMPTY            = -20016,           //
    ERR_DB_RECORD_NOT_FOUND         = -20017,           // can't find record with specified ID in DB file
    ERR_OUT_OF_DB_TABLE_BOUNDS      = -20018,           // received index is out of the DB table's bounds
    ERR_DB_CONFIGURATION_FAILED     = -20019,
    ERR_DB_OBJECT_NOT_INITIALIZED   = -20020,

    ERR_CANNOT_CREATE_RAM_DATABASE  = -20021,
    ERR_CANNOT_CLEAR_RAM_DATABASE   = -20022,            // can't free RAM memory of some DB tables
    ERR_CANNOT_RESTORE_DB_BACKUP    = -20023,
    ERR_UNREGISTERED_DB_OBJ_TYPE    = -20024,
    ERR_FILENAME_IS_EMPTY           = -20025,

    ERR_DEVICE_NOT_IN_CONFIGURING_STATE = -20026,

// -- Aux MCU
    ERR_UNREGISTERED_COMMAND        = -20030,           // bad command code to MCU or message code from MCU
    ERR_BAD_PINTYPE_FOR_COMMAND     = -20031,           // current command can't use this pin
    ERR_BAD_LEXICON                 = -20032,
    ERR_CANBUS_MSG_HANDLER_NOT_REGISTERED = -20033,     // CAN bus messages can't be processed

// --
    ERR_INVALID_PULT_CHANNEL_STATE  = -20040,
    ERR_INVALID_PULT_CHANNEL_TYPE   = -20041,
    ERR_INVALID_DEST_IP             = -20042,
    ERR_SHOULD_STOP_OPERATION       = -20043,
//    ERR_CHANNEL_SWITCHING_FAILED    = -20044,

    ERR_BAD_PKT_INDEX               = -20050,
    ERR_PREVIOUS_PKT_INDEX          = -20051,
    ERR_BAD_PKT_SENDER              = -20052,
    ERR_BAD_FRAMENUM_OR_FRAMECOUNT  = -20053,

//    ERR_UNEXPECTED_FILECODE         = -20055,
    ERR_INVALID_PARAMETER           = -20056,
    ERR_BUFFER_IS_EMPTY             = -20057,
    ERR_OUT_OF_BUFFER_BOUNDS        = -20058,

    ERR_CANNOT_ENCODE_MSG           = -20060,        // can't encode the message to send to the pult
    ERR_CANNOT_ESTABLISH_CONNECTION = -20061,        // Connection to the pult has not been established for any of the available channels
    ERR_INVALID_GPRS_STATE          = -20062,
    ERR_CANNOT_DECODE_MSG           = -20063,

    ERR_CALLBACKS_NOT_REGISTERED    = -20064,
    ERR_ALREADY_IN_PROGRESS         = -20065,
    ERR_OTHER_CMD_ALREADY_IN_PROCESSING  = -20066,       // requested action is in progress already
    ERR_ALREADY_ESTABLISHED         = -20067,
    ERR_GPRS_ACTIVATION_FAIL        = -20068,
    ERR_DIAL_PHONE_NUMBER_FAIL      = -20069,
    ERR_BAD_AT_COMMAND              = -20070,           // AT command not exist in internal "dict"

    ERR_READ_INCOMING_MSG_FAILED    = -20071,
    ERR_SHOULD_WAIT                 = -20072,        // application should wait for the end of the operation
    ERR_CHANNEL_ESTABLISHING_WOULDBLOCK    = -20073,        // app must wait till the connection will be established

    ERR_UNSOLICITED_QUERY           = -20074,       // unsolicited query from the pult was received
    ERR_SHOULD_NOT_WAIT_REPLY       = -20075,        // reply on unsolicited query from the pult was sent

    ERR_AT_COMMAND_FAILED           = -20080,       // AT command returned ERROR
    //ERR_PHONE_FUNCTIONALITY_NOT_DISABLED = -20081,
    //ERR_SIM_NOT_INSERTED            = -20082,
    ERR_DEVICE_INITIALIZATION_ERROR = -20083,
    ERR_DEVICE_INITIALIZATION_HANGUP = -20084,
    ERR_SIM_ACTIVATION_FAILED       = -20085,
    ERR_DEVICE_IS_NOT_CONFIGURED    = -20086,
    ERR_BAD_DTMF_CODE               = -20087,

    ERR_BAD_UNIT_TYPE               = -20090,
    ERR_UNDEFINED_BEHAVIOR_PRESET   = -20091,
    ERR_MAX_LEN_EXCEEDED            = -20092,
    ERR_NO_ACTIVE_CALL_FOUND        = -20093,

    ERR_SHOULD_RETRY_LATER          = -20094,
    ERR_OPERATION_NOT_IMPLEMENTED   = -20095,
    ERR_NOT_IMPLEMENTED             = -20096

} CustomerErrors;



#endif /* ERRORS_H_ */
