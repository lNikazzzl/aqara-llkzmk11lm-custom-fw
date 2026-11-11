/*
 * Portions of this file are derived from NXP software (JN-AN-1218 and the
 * JN-SW-4170 Zigbee 3.0 SDK, Copyright NXP B.V. 2017). The NXP notice follows.
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5168, JN5179].
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright NXP B.V. 2017. All rights reserved
 */

/*
 * Entry point, resources and main loop.
 * Based on the start-up sequence of NXP JN-AN-1218 (app_main.c, app_start_light.c).
 */
#include <jendefs.h>
#include "ZQueue.h"
#include "ZTimer.h"
#include "portmacro.h"
#include "zps_apl_af.h"
#include "mac_vs_sap.h"
#include "AppHardwareApi.h"
#include "AppApi.h"
#include "pwrm.h"
#include "PDM.h"
#include "pdum_gen.h"
#include "dbg.h"
#include "dbg_uart.h"
#include "recal.h"

#include "app_version.h"
#include "PDM_IDs.h"
#include "app_main.h"
#include "app_node.h"
#include "app_zcl.h"
#include "inputs.h"
#include "led.h"
#include "relay.h"

#ifdef DEBUG_APP
#define TRACE_APP TRUE
#else
#define TRACE_APP FALSE
#endif

#define TIMER_QUEUE_SIZE        8
#define MLME_QUEUE_SIZE         8
#define MCPS_QUEUE_SIZE         20
#define MCPS_DCFM_QUEUE_SIZE    5
#define BDB_QUEUE_SIZE          2
#define APP_QUEUE_SIZE          8

#define APP_ZTIMER_STORAGE      6
#define RADIO_RECAL_PERIOD      ZTIMER_TIME_SEC(60)

PUBLIC uint8 u8TimerInputs;
PUBLIC uint8 u8TimerRelay;
PUBLIC uint8 u8TimerLed;
PUBLIC uint8 u8TimerZclTick;
PUBLIC uint8 u8TimerSteering;
PUBLIC uint8 u8TimerRadioRecal;

PUBLIC tszQueue APP_msgBdbEvents;
PUBLIC tszQueue APP_msgAppEvents;

PRIVATE ZTIMER_tsTimer asTimers[APP_ZTIMER_STORAGE + BDB_ZTIMER_STORAGE];
PRIVATE zps_tsTimeEvent asTimeEvent[TIMER_QUEUE_SIZE];
PRIVATE MAC_tsMcpsVsDcfmInd asMacMcpsDcfmInd[MCPS_QUEUE_SIZE];
PRIVATE MAC_tsMlmeVsDcfmInd asMacMlmeVsDcfmInd[MLME_QUEUE_SIZE];
PRIVATE MAC_tsMcpsVsCfmData asMacMcpsDcfm[MCPS_DCFM_QUEUE_SIZE];
PRIVATE BDB_tsZpsAfEvent asBdbEvent[BDB_QUEUE_SIZE];
PRIVATE APP_tsEvent asAppEvent[APP_QUEUE_SIZE];

extern void *_stack_low_water_mark;
extern void zps_taskZPS(void);
extern void PWRM_vManagePower(void);

PRIVATE void APP_cbTimerRadioRecal(void *pvParam)
{
    if (eAHI_AttemptCalibration() == E_CAL_SUCCESS)
    {
        ZTIMER_eStart(u8TimerRadioRecal, RADIO_RECAL_PERIOD);
    }
    else
    {
        /* MAC busy, try again shortly */
        ZTIMER_eStart(u8TimerRadioRecal, ZTIMER_TIME_MSEC(10));
    }
}

PRIVATE void APP_vInitResources(void)
{
    ZTIMER_eInit(asTimers, sizeof(asTimers) / sizeof(ZTIMER_tsTimer));

    ZTIMER_eOpen(&u8TimerInputs,     INPUTS_cbTimerScan,     NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    ZTIMER_eOpen(&u8TimerRelay,      RELAY_cbTimer,          NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    ZTIMER_eOpen(&u8TimerLed,        LED_cbTimer,            NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    ZTIMER_eOpen(&u8TimerZclTick,    APP_ZCL_cbTimerTick,    NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    ZTIMER_eOpen(&u8TimerSteering,   APP_cbTimerSteering,    NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    ZTIMER_eOpen(&u8TimerRadioRecal, APP_cbTimerRadioRecal,  NULL, ZTIMER_FLAG_PREVENT_SLEEP);

    ZQ_vQueueCreate(&APP_msgBdbEvents,   BDB_QUEUE_SIZE,       sizeof(BDB_tsZpsAfEvent),    (uint8 *)asBdbEvent);
    ZQ_vQueueCreate(&APP_msgAppEvents,   APP_QUEUE_SIZE,       sizeof(APP_tsEvent),         (uint8 *)asAppEvent);
    ZQ_vQueueCreate(&zps_msgMlmeDcfmInd, MLME_QUEUE_SIZE,      sizeof(MAC_tsMlmeVsDcfmInd), (uint8 *)asMacMlmeVsDcfmInd);
    ZQ_vQueueCreate(&zps_msgMcpsDcfmInd, MCPS_QUEUE_SIZE,      sizeof(MAC_tsMcpsVsDcfmInd), (uint8 *)asMacMcpsDcfmInd);
    ZQ_vQueueCreate(&zps_TimeEvents,     TIMER_QUEUE_SIZE,     sizeof(zps_tsTimeEvent),     (uint8 *)asTimeEvent);
    ZQ_vQueueCreate(&zps_msgMcpsDcfm,    MCPS_DCFM_QUEUE_SIZE, sizeof(MAC_tsMcpsVsCfmData), (uint8 *)asMacMcpsDcfm);
}

PRIVATE void APP_vMainLoop(void)
{
    while (TRUE)
    {
        zps_taskZPS();
        bdb_taskBDB();
        ZTIMER_vTask();
        APP_vTaskNode();

        /* the watchdog resets the chip if the loop ever stalls */
        vAHI_WatchdogRestart();
        PWRM_vManagePower();
    }
}

PUBLIC void vAppMain(void)
{
    /* wait for the 32 MHz crystal, otherwise the UART runs at the wrong speed */
    while (bAHI_GetClkSource() == TRUE);
    bAHI_SetClockRate(3);

    /* outputs to a safe state before anything else */
    RELAY_vInit();
    LED_vInit();

    /* debug log on UART0 (PROG connector TxD/RxD) at 115200 */
    DBG_vUartInit(DBG_E_UART_0, DBG_E_UART_BAUD_RATE_115200);
    vAHI_SetStackOverflow(TRUE, (uint32)&_stack_low_water_mark);

    DBG_vPrintf(TRUE, "\n\n*** Aqara LLKZMK11LM custom firmware %s ***\n", APP_VERSION_STRING);
    if (bAHI_WatchdogResetEvent())
    {
        DBG_vPrintf(TRUE, "APP: reset by watchdog\n");
    }

    TARGET_INITIALISE();
    SET_IPL(0);
    portENABLE_INTERRUPTS();

    APP_vInitResources();

    /* routers never sleep, but the power manager still dozes the CPU when idle */
    PWRM_vInit(E_AHI_SLEEP_OSCON_RAMON);
    /* same EEPROM layout as the factory firmware, see PDM_IDs.h */
    PDM_eInitialise(PDM_SEGMENTS);
    PDUM_vInit();

    APP_vInitialiseNode();
    INPUTS_vInit();
    ZTIMER_eStart(u8TimerRadioRecal, RADIO_RECAL_PERIOD);

    DBG_vPrintf(TRACE_APP, "APP: BDB start\n");
    BDB_vStart();

    APP_vMainLoop();
}

/* routers do not sleep, so there are no sleep/wake callbacks */
void vAppRegisterPWRMCallbacks(void)
{
}

/* called by SDK libraries for diagnostics */
PUBLIC void vDebug(char *pcMessage)
{
    DBG_vPrintf(TRACE_APP, "%s", pcMessage);
}

PUBLIC void vDebugHex(uint32 u32Data, int iSize)
{
    DBG_vPrintf(TRACE_APP, "%x", u32Data);
}

/* interrupt vectors referenced by irq_JN516x.S that this board does not use */
PUBLIC void vISR_SystemController(void)
{
}

PUBLIC void vISR_Timer3(void)
{
    (void)u8AHI_TimerFired(E_AHI_TIMER_3);
}

PUBLIC void vISR_Timer4(void)
{
    (void)u8AHI_TimerFired(E_AHI_TIMER_4);
}
