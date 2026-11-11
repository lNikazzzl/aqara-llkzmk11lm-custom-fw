#ifndef LED_H
#define LED_H

#include <jendefs.h>

typedef enum
{
    LED_E_OFF,
    LED_E_SEARCHING,    /* blue blinks slowly while looking for a network */
    LED_E_JOINED,       /* blue on for 3 s after joining */
    LED_E_IDENTIFY,     /* blue blinks fast while Identify is active */
    LED_E_RESET,        /* red blinks fast before a factory reset */
    LED_E_ERROR,        /* red on for 3 s */
} LED_teMode;

PUBLIC void LED_vInit(void);
PUBLIC void LED_vSetMode(LED_teMode eMode);
PUBLIC LED_teMode LED_eGetMode(void);
PUBLIC void LED_cbTimer(void *pvParam);

#endif /* LED_H */
