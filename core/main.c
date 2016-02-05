#ifdef __CUSTOMER_CODE__

#include "custom_feature_def.h"
#include "ql_system.h"

#include "core/uart.h"
#include "core/atcommand.h"
#include "core/system.h"
#include "core/urc.h"
#include "core/timers.h"

#include "common/debug_macro.h"
#include "common/defines.h"

#include "db/db_serv.h"


void proc_main_task(s32 taskId)
{
    ST_MSG msg = {0};

    // -- setup communication interfaces
    Ql_UART_Register(UART_PORT1, &uart_callback_handler, NULL);
    Ql_UART_Register(UART_PORT2, &uart_callback_handler, NULL);
    Ql_UART_Register(UART_PORT3, &uart_callback_handler, NULL);

    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    Ql_UART_Open(UART_PORT2, 460800, FC_NONE);
    Ql_UART_Open(UART_PORT3, 115200, FC_NONE);

    Ql_UART_Register(VIRTUAL_PORT1, &uart_callback_handler, NULL);
    Ql_UART_Open(VIRTUAL_PORT1, 0, FC_NONE);


    // -- !!! before any debug data send to UART_2
    _RESET_DBG_BUFFER;
    REGISTER_OUT_DEBUG_MUTEX;
    REGISTER_TIMER_MUTEX;

    //
    _RESET_ONE_FILE_TRANSFER_PARAM;



    // -- greeting message
    OUT_UNPREFIXED_LINE("\r\n");
    OUT_UNPREFIXED_LINE("                    *** Hello! ***\r\n");
    OUT_UNPREFIXED_LINE("*** I'm ARMOR security system based on %s ***\r\n", GSM_MODULE_MODEL);
    OUT_UNPREFIXED_LINE("                 *** FW ver. %d.%d ***\r\n",
                          ARMOR_FW_VERSION_MAJOR, ARMOR_FW_VERSION_MINOR);
    OUT_UNPREFIXED_LINE("              *** DB ver. %s ***\r\n", DB_STRUCTURE_VERSION);
    OUT_UNPREFIXED_LINE("             *** remicollab@gmail.com ***\r\n");
    OUT_UNPREFIXED_LINE("\r\n");


    // -- and the show will go on...
    while(1)
    {
        Ql_OS_GetMessage(&msg);
        if(msg.message!=0){
            OUT_UNPREFIXED_LINE("\r\n");
        }
        switch (msg.message) {
#ifdef __OCPU_RIL_SUPPORT__
        case MSG_ID_RIL_READY:
            Ql_RIL_Initialize();
            Ar_System_StartDeviceInitialization();
            OUT_DEBUG_3("The radio interface layer has been initialized\r\n");
            break;
#endif  // __OCPU_RIL_SUPPORT__

        case MSG_ID_URC_INDICATION:
        {
            RIL_URC_Handler(msg.param1,msg.param2);
            break;
        }

        case MSG_ID_START_SINGLE_TIMER:
            OUT_DEBUG_8("MSG_ID_START_SINGLE_TIMER #%s from task %d\r\n", Ar_Timer_getTimerNameByCode(msg.param1), msg.srcTaskId)
            Ar_Timer_startSingle((ArmorTimer)msg.param1, msg.param2);
            break;

        case MSG_ID_START_CONTINUOUS_TIMER:
            OUT_DEBUG_8("MSG_ID_START_CONTINUOUS_TIMER #%s from task %d\r\n", Ar_Timer_getTimerNameByCode(msg.param1), msg.srcTaskId)
            Ar_Timer_startContinuous((ArmorTimer)msg.param1, msg.param2);
            break;

        case MSG_ID_STOP_TIMER:
            OUT_DEBUG_8("MSG_ID_STOP_TIMER #%s from task %d\r\n", Ar_Timer_getTimerNameByCode(msg.param1), msg.srcTaskId)
            Ar_Timer_stop((ArmorTimer)msg.param1);
            break;

        case MSG_ID_GPRS_RX:
        {
            OUT_DEBUG_8("MSG_ID_GPRS_RX from task %d\r\n", msg.srcTaskId);
            break;
        }

        case 0:
            //OUT_UNPREFIXED_LINE(".");
            break;
        default:
            OUT_DEBUG_1("<<< Unhandled MSG: id=%#x, srcTaskId=%d, param1=%d, param2=%d >>>\r\n",
                        msg.message, msg.srcTaskId, msg.param1, msg.param2);
        }
    }
}


#endif  // __CUSTOMER_CODE__
