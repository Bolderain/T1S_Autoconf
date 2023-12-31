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

#include "app.h"
#include "bc_com.h"
#include "system/console/sys_console.h"
#include "config/FreeRTOS/driver/lan865x/drv_lan865x.h"




/* converts field descriptor to a field width */
#define FIELD_WIDTH(fd) ((fd)&255)

/* converts field descriptor to a field offset */
#define FIELD_OFFSET(fd) ((fd) >> 8 & 255)

/* converts field descriptor to bit mask of a field, like 0b0110 */
#define FIELD_MASK(fd) (((1u << FIELD_WIDTH(fd)) - 1) << FIELD_OFFSET(fd))

/* converts field offset and field width to a field descriptor */
#define FIELD_DESCRIPTOR(offset, width) ((offset) << 8 | (width))

/* converts register mask to a field value */
#define R2F(reg, fd) (((unsigned)(reg)&FIELD_MASK(fd)) >> FIELD_OFFSET(fd))

/* converts field value to a register mask */
/* may only be used for register initialization */
#define F2R_(val, fd) (((unsigned)(val) << FIELD_OFFSET(fd)) & FIELD_MASK(fd))

/* returns modified value reg for a field descriptor fd */
#define F2R(val, fd, reg) (((reg) & ~FIELD_MASK(fd)) | F2R_(val, fd))



#define __APP_DEBUG_PRINT 
#ifdef __APP_DEBUG_PRINT
#define APP_DEBUG_PRINT(fmt, ...)  SYS_CONSOLE_PRINT(fmt, ##__VA_ARGS__)
#else
#define APP_DEBUG_PRINT(fmt, ...)
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
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
 */

APP_DATA appData;

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

void APP_TimerCallback(uintptr_t context);
void APP_Print_State_Change(void);

/* TODO:  Add any necessary local functions.
 */


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void) {
    appData.state = APP_STATE_INIT;
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void) {

    APP_Print_State_Change();

    switch (appData.state) {
        case APP_STATE_INIT:
            appData.countdown = 10;
            appData.timer_client_hdl = SYS_TIME_TimerCreate(0, SYS_TIME_MSToCount(100), &APP_TimerCallback, (uintptr_t) NULL, SYS_TIME_PERIODIC);
            SYS_TIME_TimerStart(appData.timer_client_hdl);
            appData.state = APP_STATE_START_REQUEST;
            break;

        case APP_STATE_START_REQUEST:
            if (appData.countdown == 0) {
                APP_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
                BC_COM_send((uint8_t*) & auto_conf_msg_transmit, sizeof (AUTOCONFMSG));
                appData.countdown = 50;
                appData.state = APP_STATE_WAIT_FOR_REQUEST_TO_BE_SENT;
            }
            break;

        case APP_STATE_WAIT_FOR_REQUEST_TO_BE_SENT:
            if (BC_COM_is_data_send() == true) {
                BC_COM_stop_send();
                appData.state = APP_STATE_WAIT_FOR_IDLE_TO_START_LISTENING;
            }
            if (appData.countdown == 0) {
                APP_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
                BC_COM_stop_send();
                appData.state = APP_STATE_DECIDE_TO_BE_CONTROL_NODE;
            }
            break;

        case APP_STATE_WAIT_FOR_IDLE_TO_START_LISTENING:
            if (BC_COM_is_idle() == true) {
                BC_COM_listen(sizeof (AUTOCONFMSG));
                appData.countdown = 50;
                appData.state = APP_STATE_WAIT_FOR_REQUESTED_ANSWER;
            }
            break;

        case APP_STATE_WAIT_FOR_REQUESTED_ANSWER:
            if (BC_COM_is_data_received() == true) {
                BC_COM_read_data((uint8_t *) & auto_conf_msg_receive);
                BC_COM_stop_listen();
                appData.state = APP_STATE_PROCESS_REQUESTED_DATA;
                break;
            }
            if (appData.countdown == 0) {
                APP_DEBUG_PRINT("Timeout %s %d\n\r", __FILE__, __LINE__);
                BC_COM_stop_listen();
                uint16_t data;
                LAN867X_REG_OBJ clientObj = {0};
                data = F2R_(0, PHY_PLCA_CTRL1_ID0) | F2R_(5, PHY_PLCA_CTRL1_NCNT);
                opRes = Write_Phy_Register(&clientObj, 0, 0x0004CA02u /* PLCA_CONTROL_1_REGISTER */, data);

                DRV_LAN865X_WriteRegister(0, uint32_t addr, uint32_t value, false, NULL, NULL);

                appData.state = APP_STATE_DECIDE_TO_BE_CONTROL_NODE;
            }
            break;

        case APP_STATE_PROCESS_REQUESTED_DATA:
            APP_DEBUG_PRINT("APP: Requested data received\n\r");
            appData.state = APP_STATE_IDLE;
            break;

        case APP_STATE_DECIDE_TO_BE_CONTROL_NODE:
            if (BC_COM_is_idle() == true) {
                BC_COM_listen(sizeof (AUTOCONFMSG));
                appData.state = APP_STATE_CONTROL_NODE_WAIT_FOR_REQUEST;
            }
            break;

        case APP_STATE_CONTROL_NODE_WAIT_FOR_REQUEST:
            if (BC_COM_is_data_received() == true) {
                BC_COM_read_data((uint8_t *) & auto_conf_msg_receive);
                BC_COM_stop_listen();
                appData.state = APP_STATE_CONTROL_NODE_ANSWER_REQUEST;
            }
            break;

        case APP_STATE_CONTROL_NODE_ANSWER_REQUEST:
            if (BC_COM_is_idle() == true) {
                memcpy((void*) &auto_conf_msg_transmit, (void*) &auto_conf_msg_receive, sizeof (AUTOCONFMSG));
                auto_conf_msg_transmit.nodeid++;
                BC_COM_send((uint8_t*) & auto_conf_msg_transmit, sizeof (AUTOCONFMSG));
                appData.state = APP_STATE_CONTROL_NODE_RESTART_LISTENING;
            }
            break;

        case APP_STATE_CONTROL_NODE_RESTART_LISTENING:
            if (BC_COM_is_data_send()) {
                BC_COM_stop_listen();
                appData.state = APP_STATE_DECIDE_TO_BE_CONTROL_NODE;
            }
            break;

        case APP_STATE_IDLE:
            break;

        default:
        {
            APP_DEBUG_PRINT("APP: I should be never here: %s %d\n\r", __FILE__, __LINE__);
            while (1);
            break;
        }
    }
}

void APP_TimerCallback(uintptr_t context) {
    if (appData.countdown) {
        appData.countdown--;
    }
}

char *app_states_str[] = {
    "APP_STATE_INIT",
    "APP_STATE_START_REQUEST",
    "APP_STATE_WAIT_FOR_REQUEST_TO_BE_SENT",
    "APP_STATE_WAIT_FOR_IDLE_TO_START_LISTENING",
    "APP_STATE_WAIT_FOR_REQUESTED_ANSWER",
    "APP_STATE_PROCESS_REQUESTED_DATA",
    "APP_STATE_DECIDE_TO_BE_CONTROL_NODE",
    "APP_STATE_CONTROL_NODE_WAIT_FOR_REQUEST",
    "APP_STATE_CONTROL_NODE_ANSWER_REQUEST",
    "APP_STATE_CONTROL_NODE_RESTART_LISTENING",
    "APP_STATE_IDLE",
    "APP_VOID"
};

void APP_Print_State_Change(void) {
    static APP_STATES states = APP_VOID;
    if (states != appData.state) {
        states = appData.state;
        APP_DEBUG_PRINT("%s\n\r", app_states_str[states]);
    }
}

/*******************************************************************************
 End of File
 */
