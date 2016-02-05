#ifndef DEFINES_H
#define DEFINES_H

// -- firmware version
#ifndef ARMOR_FW_VERSION
#define ARMOR_FW_VERSION_MAJOR  0
#define ARMOR_FW_VERSION_MINOR  1
#endif

#define GSM_MODULE_MODEL     "Quectel M85"
#define DB_STRUCTURE_VERSION "1409241211"  // YYMMDDhhmm

#define DB_VALUE_NULL  -1

#define PULT_FIFO_CAPACITY              100     //  capacity of the pult's FIFO buffer
#define ZONE_DAMAGED_THRESHOLD          10

#define SIMCARD_BLOCKING_THRESHOLD      3
#define PARENT_DEVICE_SILENCE_THRESHOLD 30//300 // sec
#define PARENT_DEVICE_SILENCE_AFTER_POWERON_THRESHOLD 5 // sec

#define DTMF_PULSE_DEFAULT_LEN          120 // ms

#define MAX_DEBUG_LEVEL                 10
#define DEBUG_BUF_SIZE                  10000
#define GPRS_RX_BUFFER_SIZE             1024
#define TEMP_TX_BUFFER_SIZE             1024

#define CAN_MESSAGE_BUFFER_SIZE         100
#define CAN_DATA_BYTES_PER_PKT          5

#define CODEC_RX_BUFFER_SIZE            1024
#define CODEC_TX_BUFFER_SIZE            1024

#define MCU_MESSAGE_MAX_SIZE            40
#define CONFIGURATOR_MESSAGE_MAX_SIZE   1024

#define TIMER_NORMAL_SENT_INTERVAL      20000
#define SEND_ATTEMPTS_QTY               3
#define TIMER_CONNECTION_CHECK_INTERVAL 30000 //(TIMER_NORMAL_SENT_INTERVAL + 100)
#define CONFIGURATION_ROLLBACK_INTERVAL (TIMER_CONNECTION_CHECK_INTERVAL + SEND_ATTEMPTS_QTY * \
                                         TIMER_NORMAL_SENT_INTERVAL + 100 )

#define STARTUP_INIT_TIMER_INTERVAL     1000
#define SWITCH_SIMCARD_TIMER_INTERVAL   1000
#define SETUP_CONNECTION_TIMER_INTERVAL 1000
#define DTMF_PREDELAY                   1000

//#define NO_SEND_MSG        // do not send any messages to the pult
//#define NO_SEND_HEARTBEAT  // parent units send special test pkt to GSM-module periodically
#define NO_POWER_OUT_DEBUG  // disable ADC measuring information
//#define NO_USE_SYSTEM_LED_AS_ARMED //
//#define SEND_ONLY_SQLITE_DB        //  send all files
//#define NO_PERIODICAL_BLOCK  // disable send to terminal test block

#define NO_CB  // CircularBuffer не показывать пока раскоментарино



// -- packet type marker
#define MCU_PACKET_MARKER                       186   // 0xBA
#define CONFIGURATION_PACKET_MARKER             254   // 0xFE
#define CONFIGURATION_AUTOSYNC_PACKET_MARKER    253   // 0xFD

#define CRC_BYTE        0x00   // reserved place for CRC

#define ALIAS_LEN       30   // length of aliases for any objects
#define PHONE_LEN       13   // length of phone numbers
#define APN_LEN         30   // length of APN name
#define CREDENTIALS_LEN 30   // login and password
#define IPADDR_LEN      4
#define TARGET_ADDRESS_LEN 30
#define SYSTEM_INFO_STRINGS_LEN 30

#define KEYBOARD_MAX_KEY_SIZE       4
#define TOUCHMEMORY_MAX_KEY_SIZE    8

#define PROTOCOL_ARMOR_MAX_MSG_LEN 87   // CAN-limit Alexa
#define PROTOCOL_ARMOR_MAX_CYCLE_COUNTER   99
#define PROTOCOL_ARMOR_VERSION 0x01


// --
#define PROCESS_ALARMED_ZONES_INTERVAL  5000 // ms

#define PROCESS_SOFTWARE_TIMERS_INTERVAL 1000 // ms

// -- gprs network params
#define GPRS_NORMAL_TIMER                   800   // ms
#define GPRS_ACTIVATE_ITERATION_THRESHOLD   60 // iterations
#define SOCKET_INIT_VALUE    -1

#define PSS_WAITING_ITERATION   100 // iterations


#define VOICECALL_NO_RING_HANGUP_INTERVAL     15000 // ms
#define VOICECALL_NO_ANSWER_HANGUP_INTERVAL   6000 // ms
#define VOICECALL_DIALING_ATTEMPTS_THRESHOLD  3

#define TOUCHMEMORY_READER_BLOCK_INTERVAL   1000 // ms

// -- power ADC
/* y = A*x + B */
#define ADC_DEFAULT_A   65      // tangent
#define ADC_DEFAULT_B   1214    // shift

#define ADC_DELTA_PERCENT               1 // %
#define ADC_SAMPLING_INTERVAL           1 // sec
#define BATTERY_CHARGING_INTERVAL       300 // sec

/* all in Volts */
#define BAT_DEAD        7.0     // dead
#define BAT_CRITICAL    9.5     // report to pult and power off
#define BAT_LO          12.0    // begin charging
#define BAT_HI          13.8    // end charging


// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define LEN_OF(arr) (sizeof(arr)/sizeof(arr[0]))
#define ARRAY_SIZE LEN_OF

#define STRING_LEN (sizeof(arr)/sizeof(arr[0] - 1))
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


#endif // DEFINES_H
