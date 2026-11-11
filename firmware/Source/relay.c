/*
 * Driver for the two bistable (latching) relays.
 *
 * Each relay has a set and a reset coil switched by a MOSFET. A coil only
 * needs a short pulse; holding it would waste power and heat the coil. Only
 * one coil is energised at a time to keep the load on the supply low.
 */
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "dbg.h"
#include "hw.h"
#include "relay.h"
#include "app_main.h"

#ifdef DEBUG_RELAY
#define TRACE_RELAY TRUE
#else
#define TRACE_RELAY FALSE
#endif

PRIVATE const uint32 au32OnPin[RELAY_CHANNELS]  = { RELAY1_ON,  RELAY2_ON  };
PRIVATE const uint32 au32OffPin[RELAY_CHANNELS] = { RELAY1_OFF, RELAY2_OFF };

PRIVATE bool_t abTarget[RELAY_CHANNELS];
PRIVATE uint8 u8Pending;
PRIVATE bool_t bBusy;

PRIVATE void vStartNext(void)
{
    uint8 u8Ch;

    for (u8Ch = 0; u8Ch < RELAY_CHANNELS; u8Ch++)
    {
        if (u8Pending & (1 << u8Ch))
        {
            u8Pending &= ~(1 << u8Ch);
            bBusy = TRUE;
            DBG_vPrintf(TRACE_RELAY, "RELAY: ch%d %s\n", u8Ch + 1, abTarget[u8Ch] ? "on" : "off");
            vAHI_DioSetOutput(abTarget[u8Ch] ? au32OnPin[u8Ch] : au32OffPin[u8Ch], 0);
            ZTIMER_eStart(u8TimerRelay, ZTIMER_TIME_MSEC(RELAY_PULSE_MS));
            return;
        }
    }
}

PUBLIC void RELAY_vInit(void)
{
    vAHI_DioSetOutput(0, RELAY_ALL);
    vAHI_DioSetDirection(0, RELAY_ALL);
    u8Pending = 0;
    bBusy = FALSE;
}

PUBLIC void RELAY_vSet(uint8 u8Channel, bool_t bOn)
{
    if (u8Channel >= RELAY_CHANNELS)
    {
        return;
    }
    abTarget[u8Channel] = bOn;
    u8Pending |= (1 << u8Channel);
    if (!bBusy)
    {
        vStartNext();
    }
}

PUBLIC void RELAY_cbTimer(void *pvParam)
{
    vAHI_DioSetOutput(0, RELAY_ALL);
    bBusy = FALSE;
    vStartNext();
}
