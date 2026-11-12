/*
 * Polled inputs: the on-board pairing button and the S1/S2 wall switch inputs.
 *
 * All three are active low with external pull-ups. They are sampled every
 * INPUT_SCAN_MS and a level must be stable for INPUT_DEBOUNCE_MS to count.
 */
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "ZQueue.h"
#include "dbg.h"
#include "hw.h"
#include "inputs.h"
#include "app_main.h"

#ifdef DEBUG_INPUTS
#define TRACE_INPUTS TRUE
#else
#define TRACE_INPUTS FALSE
#endif

#define NUM_INPUTS          3
#define IDX_BUTTON          0
#define IDX_S1              1
#define IDX_S2              2

#define DEBOUNCE_TICKS      (INPUT_DEBOUNCE_MS / INPUT_SCAN_MS)
#define LONG_PRESS_TICKS    (BTN_LONG_PRESS_MS / INPUT_SCAN_MS)
#define REPAIR_WINDOW_TICKS (SWITCH_REPAIR_WINDOW_MS / INPUT_SCAN_MS)

PRIVATE const uint32 au32Pin[NUM_INPUTS] = { BTN_PAIR, IN_S1, IN_S2 };

PRIVATE bool_t abActive[NUM_INPUTS];    /* debounced state, TRUE = closed */
PRIVATE uint8 au8Stable[NUM_INPUTS];    /* ticks the raw level differs from abActive */
PRIVATE uint16 u16HeldTicks;
PRIVATE bool_t bLongSent;
PRIVATE uint32 u32Ticks;                /* scan ticks since start */
PRIVATE uint32 au32SwitchTicks[SWITCH_REPAIR_COUNT];   /* ring of the last switch operations */
PRIVATE uint8 u8SwitchCount;
PRIVATE uint8 u8SwitchNext;

PRIVATE void vPost(APP_teEventType eType, uint8 u8Channel)
{
    APP_tsEvent sEvent;

    sEvent.eType = eType;
    sEvent.u8Channel = u8Channel;
    ZQ_bQueueSend(&APP_msgAppEvents, &sEvent);
}

/* TRUE when the last SWITCH_REPAIR_COUNT operations fit in the window */
PRIVATE bool_t bSwitchRepairGesture(void)
{
    uint32 u32Oldest;

    au32SwitchTicks[u8SwitchNext] = u32Ticks;
    u8SwitchNext = (u8SwitchNext + 1) % SWITCH_REPAIR_COUNT;
    if (u8SwitchCount < SWITCH_REPAIR_COUNT)
    {
        u8SwitchCount++;
    }
    if (u8SwitchCount < SWITCH_REPAIR_COUNT)
    {
        return FALSE;
    }

    /* the ring holds the last SWITCH_REPAIR_COUNT operations, u8SwitchNext is the oldest */
    u32Oldest = au32SwitchTicks[u8SwitchNext];
    if (u32Ticks - u32Oldest > REPAIR_WINDOW_TICKS)
    {
        return FALSE;
    }
    u8SwitchCount = 0;
    return TRUE;
}

PRIVATE void vOnChange(uint8 u8Idx, bool_t bActive)
{
    DBG_vPrintf(TRACE_INPUTS, "INPUT: %d -> %d\n", u8Idx, bActive);

    if (u8Idx == IDX_BUTTON)
    {
        if (bActive)
        {
            u16HeldTicks = 0;
            bLongSent = FALSE;
        }
        else if (!bLongSent)
        {
            vPost(APP_E_EVENT_BUTTON_SHORT, 0);
        }
        return;
    }

    if (INPUT_ROCKER_MODE || bActive)
    {
        vPost(APP_E_EVENT_SWITCH, (u8Idx == IDX_S1) ? 0 : 1);
        if (bSwitchRepairGesture())
        {
            DBG_vPrintf(TRUE, "INPUT: %d switch operations in %d s, re-pairing\n",
                        SWITCH_REPAIR_COUNT, SWITCH_REPAIR_WINDOW_MS / 1000);
            /* same as holding the on-board button: leave, reset, look for a network */
            vPost(APP_E_EVENT_BUTTON_LONG, 0);
        }
    }
}

PUBLIC void INPUTS_vInit(void)
{
    uint32 u32Dio;
    uint8 i;

    vAHI_DioSetDirection(BTN_PAIR | IN_S1 | IN_S2, 0);
    /* the S1/S2 pull-ups are on the power board; keep the inputs defined
       when the MCU module runs on its own */
    vAHI_DioSetPullup(BTN_PAIR | IN_S1 | IN_S2, 0);

    /* take the current levels as the starting state so nothing fires at boot */
    u32Dio = u32AHI_DioReadInput();
    for (i = 0; i < NUM_INPUTS; i++)
    {
        abActive[i] = (u32Dio & au32Pin[i]) == 0;
        au8Stable[i] = 0;
    }
    bLongSent = abActive[IDX_BUTTON];

    ZTIMER_eStart(u8TimerInputs, ZTIMER_TIME_MSEC(INPUT_SCAN_MS));
}

PUBLIC bool_t INPUTS_bPairButtonHeld(void)
{
    return (u32AHI_DioReadInput() & BTN_PAIR) == 0;
}

PUBLIC void INPUTS_cbTimerScan(void *pvParam)
{
    uint32 u32Dio = u32AHI_DioReadInput();
    uint8 i;

    ZTIMER_eStart(u8TimerInputs, ZTIMER_TIME_MSEC(INPUT_SCAN_MS));
    u32Ticks++;

    for (i = 0; i < NUM_INPUTS; i++)
    {
        bool_t bRaw = (u32Dio & au32Pin[i]) == 0;

        if (bRaw == abActive[i])
        {
            au8Stable[i] = 0;
        }
        else if (++au8Stable[i] >= DEBOUNCE_TICKS)
        {
            au8Stable[i] = 0;
            abActive[i] = bRaw;
            vOnChange(i, bRaw);
        }
    }

    if (abActive[IDX_BUTTON] && !bLongSent)
    {
        if (++u16HeldTicks >= LONG_PRESS_TICKS)
        {
            bLongSent = TRUE;
            vPost(APP_E_EVENT_BUTTON_LONG, 0);
        }
    }
}
