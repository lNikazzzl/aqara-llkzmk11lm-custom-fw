#ifndef APP_NODE_H
#define APP_NODE_H

#include <jendefs.h>

typedef enum
{
    E_STARTUP,      /* factory new, not on a network */
    E_NFN_START,    /* restarting with a saved network */
    E_RUNNING,      /* joined */
} teNodeState;

PUBLIC void APP_vInitialiseNode(void);
PUBLIC void APP_vTaskNode(void);
PUBLIC void APP_cbTimerSteering(void *pvParam);
PUBLIC bool_t APP_bIsRunning(void);

#endif /* APP_NODE_H */
