/*
 * Status LEDs (blue and red) on the MCU module.
 */
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "hw.h"
#include "led.h"
#include "app_main.h"

#define LED_TICK_MS         100
#define LED_TIMEOUT_TICKS   30

PRIVATE LED_teMode eLedMode = LED_E_OFF;
PRIVATE uint8 u8Tick;

PRIVATE void vLeds(bool_t bBlue, bool_t bRed)
{
    /* active low */
    vAHI_DioSetOutput((bBlue ? 0 : LED_BLUE) | (bRed ? 0 : LED_RED),
                      (bBlue ? LED_BLUE : 0) | (bRed ? LED_RED : 0));
}

PUBLIC void LED_vInit(void)
{
    vLeds(FALSE, FALSE);
    vAHI_DioSetDirection(0, LED_BLUE | LED_RED);
}

PUBLIC void LED_vSetMode(LED_teMode eMode)
{
    eLedMode = eMode;
    u8Tick = 0;
    ZTIMER_eStop(u8TimerLed);
    LED_cbTimer(NULL);
}

PUBLIC LED_teMode LED_eGetMode(void)
{
    return eLedMode;
}

PUBLIC void LED_cbTimer(void *pvParam)
{
    u8Tick++;

    switch (eLedMode)
    {
    case LED_E_SEARCHING:
        vLeds((u8Tick % 10) < 5, FALSE);
        break;

    case LED_E_IDENTIFY:
        vLeds((u8Tick % 4) < 2, FALSE);
        break;

    case LED_E_RESET:
        vLeds(FALSE, (u8Tick % 2) == 0);
        break;

    case LED_E_JOINED:
    case LED_E_ERROR:
        if (u8Tick > LED_TIMEOUT_TICKS)
        {
            eLedMode = LED_E_OFF;
            vLeds(FALSE, FALSE);
            return;
        }
        vLeds(eLedMode == LED_E_JOINED, eLedMode == LED_E_ERROR);
        break;

    case LED_E_OFF:
    default:
        vLeds(FALSE, FALSE);
        return;
    }

    ZTIMER_eStart(u8TimerLed, ZTIMER_TIME_MSEC(LED_TICK_MS));
}
