/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "bc_com.h"
#include "bc_test.h"
#include "system/console/sys_console.h"
#include "config/FreeRTOS/library/tcpip/tcpip.h"


#define __BC_TEST_DEBUG_PRINT 
#ifdef __BC_TEST_DEBUG_PRINT
#define BC_TEST_DEBUG_PRINT(fmt, ...)  SYS_CONSOLE_PRINT(fmt, ##__VA_ARGS__)
#else
#define BC_TEST_DEBUG_PRINT(fmt, ...)
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

typedef struct {
    IPV6_ADDR ip6;
    IPV4_ADDR ip4;
    TCPIP_MAC_ADDR mac;
    uint8_t nodeid;
    uint8_t maxnodeid;
    uint8_t randommssg[20];
} AUTOCONFMSG;

AUTOCONFMSG auto_conf_msg_transmit;
AUTOCONFMSG auto_conf_msg_receive;

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the BC_TEST_Initialize function.

    Application strings and buffers are be defined outside this structure.
 */

BC_TEST_DATA bc_test;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary callback functions.
 */

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

void BC_TEST_TimerCallback(uintptr_t context);
void BC_TEST_Print_State_Change(void);

/* TODO:  Add any necessary local functions.
 */


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void BC_TEST_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void BC_TEST_Initialize(void) {
    bc_test.state = BC_TEST_STATE_INIT;
}

/******************************************************************************
  Function:
    void BC_TEST_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void BC_TEST_Tasks(void) {

    BC_TEST_Print_State_Change();

    switch (bc_test.state) {
        case BC_TEST_STATE_INIT:
            bc_test.countdown = 10;
            bc_test.timer_client_hdl = SYS_TIME_TimerCreate(0, SYS_TIME_MSToCount(100), &BC_TEST_TimerCallback, (uintptr_t) NULL, SYS_TIME_PERIODIC);
            SYS_TIME_TimerStart(bc_test.timer_client_hdl);
            bc_test.state = BC_TEST_STATE_START_REQUEST;
            break;

        case BC_TEST_STATE_START_REQUEST:
            if (bc_test.countdown == 0) {
                BC_TEST_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
#ifdef MY_NODE_0
                BC_TEST_DEBUG_PRINT("MY_NODE_0 %s %s\n\r",__DATE__,__TIME__);
                bc_test.state = BC_TEST_STATE_DECIDE_TO_BE_CONTROL_NODE;
#endif
#ifdef MY_NODE_1
                BC_TEST_DEBUG_PRINT("MY_NODE_1 %s %s\n\r",__DATE__,__TIME__);
                bc_test.state = BC_TEST_STATE_WAIT_FOR_REQUEST_TO_BE_SENT;
                BC_TEST_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
                BC_COM_send((uint8_t*) & auto_conf_msg_transmit, sizeof (AUTOCONFMSG));
                bc_test.countdown = 50;                     
#endif                                           
            }
            break;

        case BC_TEST_STATE_WAIT_FOR_REQUEST_TO_BE_SENT:
            if (BC_COM_is_data_send() == true) {
                BC_COM_stop_send();
                bc_test.state = BC_TEST_STATE_WAIT_FOR_IDLE_TO_START_LISTENING;
            }
            if (bc_test.countdown == 0) {
                BC_TEST_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
                BC_COM_stop_send();
                bc_test.state = BC_TEST_STATE_DECIDE_TO_BE_CONTROL_NODE;
            }
            break;

        case BC_TEST_STATE_WAIT_FOR_IDLE_TO_START_LISTENING:
            if (BC_COM_is_idle() == true) {
                BC_COM_listen(sizeof (AUTOCONFMSG));
                bc_test.countdown = 50;
                bc_test.state = BC_TEST_STATE_WAIT_FOR_REQUESTED_ANSWER;
            }
            break;

        case BC_TEST_STATE_WAIT_FOR_REQUESTED_ANSWER:
            if (BC_COM_is_data_received() == true) {
                BC_COM_read_data((uint8_t *) & auto_conf_msg_receive);
                BC_COM_stop_listen();
                bc_test.state = BC_TEST_STATE_PROCESS_REQUESTED_DATA;
                break;
            }
            if (bc_test.countdown == 0) {
                BC_TEST_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
                BC_COM_stop_listen();


                bc_test.state = BC_TEST_STATE_DECIDE_TO_BE_CONTROL_NODE;
            }
            break;

        case BC_TEST_STATE_PROCESS_REQUESTED_DATA:
            BC_TEST_DEBUG_PRINT("BC_TEST: Requested data received\n\r");
            bc_test.state = BC_TEST_STATE_IDLE;
            break;

        case BC_TEST_STATE_DECIDE_TO_BE_CONTROL_NODE:
            if (BC_COM_is_idle() == true) {
                BC_COM_listen(sizeof (AUTOCONFMSG));
                bc_test.state = BC_TEST_STATE_CONTROL_NODE_WAIT_FOR_REQUEST;
            }
            break;

        case BC_TEST_STATE_CONTROL_NODE_WAIT_FOR_REQUEST:
            if (BC_COM_is_data_received() == true) {
                BC_TEST_DEBUG_PRINT("BC_TEST: Requested data received\n\r");
                BC_COM_read_data((uint8_t *) & auto_conf_msg_receive);
                BC_COM_stop_listen();
                bc_test.state = BC_TEST_STATE_CONTROL_NODE_ANSWER_REQUEST;
            }
            break;

        case BC_TEST_STATE_CONTROL_NODE_ANSWER_REQUEST:
            if (BC_COM_is_idle() == true) {
                memcpy((void*) &auto_conf_msg_transmit, (void*) &auto_conf_msg_receive, sizeof (AUTOCONFMSG));
                auto_conf_msg_transmit.nodeid++;
                BC_COM_send((uint8_t*) & auto_conf_msg_transmit, sizeof (AUTOCONFMSG));
                bc_test.state = BC_TEST_STATE_CONTROL_NODE_RESTART_LISTENING;
            }
            break;

        case BC_TEST_STATE_CONTROL_NODE_RESTART_LISTENING:
            if (BC_COM_is_data_send()) {
                BC_COM_stop_listen();
                bc_test.state = BC_TEST_STATE_DECIDE_TO_BE_CONTROL_NODE;
            }
            break;

        case BC_TEST_STATE_IDLE:
            break;

        default:
        {
            BC_TEST_DEBUG_PRINT("APP: I should be never here: %s %d\n\r", __FILE__, __LINE__);
            while (1);
            break;
        }
    }
}

void BC_TEST_TimerCallback(uintptr_t context) {
    if (bc_test.countdown) {
        bc_test.countdown--;
    }
}

char *app_states_str[] = {
    "BC_TEST_STATE_INIT",
    "BC_TEST_STATE_START_REQUEST",
    "BC_TEST_STATE_WAIT_FOR_REQUEST_TO_BE_SENT",
    "BC_TEST_STATE_WAIT_FOR_IDLE_TO_START_LISTENING",
    "BC_TEST_STATE_WAIT_FOR_REQUESTED_ANSWER",
    "BC_TEST_STATE_PROCESS_REQUESTED_DATA",
    "BC_TEST_STATE_DECIDE_TO_BE_CONTROL_NODE",
    "BC_TEST_STATE_CONTROL_NODE_WAIT_FOR_REQUEST",
    "BC_TEST_STATE_CONTROL_NODE_ANSWER_REQUEST",
    "BC_TEST_STATE_CONTROL_NODE_RESTART_LISTENING",
    "BC_TEST_STATE_IDLE",
    "BC_TEST_VOID"
};

void BC_TEST_Print_State_Change(void) {
    static BC_TEST_STATES states = BC_TEST_VOID;
    if (states != bc_test.state) {
        states = bc_test.state;
        BC_TEST_DEBUG_PRINT("%s\n\r", app_states_str[states]);
    }
}

/*******************************************************************************
 End of File
 */