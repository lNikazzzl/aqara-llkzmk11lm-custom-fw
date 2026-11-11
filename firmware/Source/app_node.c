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
 * Network state: joining, rejoining, leaving and factory reset.
 * Based on app_zlo_light_node.c from NXP JN-AN-1218 without touchlink and green power.
 *
 * Whether we are on a network is taken from the stack records, which are
 * shared with the factory firmware, so nothing about it is stored separately.
 */
#include <jendefs.h>
#include <string.h>
#include "dbg.h"
#include "pdum_gen.h"
#include "zps_gen.h"
#include "zps_apl.h"
#include "zps_apl_af.h"
#include "zps_apl_zdo.h"
#include "zps_apl_aib.h"
#include "zps_nwk_nib.h"
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "ZQueue.h"
#include "bdb_api.h"
#include "bdb_options.h"

#include "app_main.h"
#include "app_node.h"
#include "app_ota.h"
#include "app_zcl.h"
#include "led.h"

#ifdef DEBUG_NODE
#define TRACE_NODE TRUE
#else
#define TRACE_NODE FALSE
#endif

#define ZDO_ENDPOINT            0
/* a factory new device keeps looking for a network for about 10 minutes */
#define STEERING_RETRY_TIME     ZTIMER_TIME_SEC(30)
#define STEERING_MAX_ATTEMPTS   20

PRIVATE teNodeState eNodeState = E_STARTUP;
PRIVATE uint8 u8SteeringAttempts;

PRIVATE void vStartSteering(void)
{
    DBG_vPrintf(TRACE_NODE, "NODE: network steering, attempt %d\n", u8SteeringAttempts + 1);
    LED_vSetMode(LED_E_SEARCHING);
    if (BDB_eNsStartNwkSteering() != BDB_E_SUCCESS)
    {
        /* BDB busy, try again later */
        ZTIMER_eStart(u8TimerSteering, STEERING_RETRY_TIME);
    }
}

PRIVATE void vSetRunning(void)
{
    eNodeState = E_RUNNING;
    ZTIMER_eStop(u8TimerSteering);
}

PRIVATE void vFactoryResetAndRestart(void)
{
    DBG_vPrintf(TRUE, "NODE: factory reset\n");

    eNodeState = E_STARTUP;
    sBDB.sAttrib.bbdbNodeIsOnANetwork = FALSE;

    /* forget the network; the relay settings in app_store are kept */
    ZPS_eAplAibSetApsUseExtendedPanId(0);
    ZPS_vDefaultStack();
    ZPS_vSetKeys();
    ZPS_vSaveAllZpsRecords();
    APP_OTA_vResetPersist();

    vAHI_SwReset();
}

PRIVATE void vLeaveAndReset(void)
{
    LED_vSetMode(LED_E_RESET);
    if ((eNodeState == E_RUNNING) &&
        (ZPS_eAplZdoLeaveNetwork(0UL, FALSE, FALSE) == ZPS_E_SUCCESS))
    {
        /* the reset happens on ZPS_EVENT_NWK_LEAVE_CONFIRM */
        return;
    }
    vFactoryResetAndRestart();
}

PRIVATE void vHandleZdoEvent(ZPS_tsAfEvent *psStackEvent)
{
    switch (psStackEvent->eType)
    {
    case ZPS_EVENT_NWK_JOINED_AS_ROUTER:
        DBG_vPrintf(TRACE_NODE, "NODE: joined as router, addr %04x\n",
                    psStackEvent->uEvent.sNwkJoinedEvent.u16Addr);
        ZPS_eAplAibSetApsUseExtendedPanId(ZPS_u64NwkNibGetEpid(ZPS_pvAplZdoGetNwkHandle()));
        vSetRunning();
        break;

    case ZPS_EVENT_NWK_LEAVE_INDICATION:
        if ((psStackEvent->uEvent.sNwkLeaveIndicationEvent.u64ExtAddr == 0UL) &&
            (psStackEvent->uEvent.sNwkLeaveIndicationEvent.u8Rejoin == 0))
        {
            /* the coordinator removed us from the network */
            vFactoryResetAndRestart();
        }
        break;

    case ZPS_EVENT_NWK_LEAVE_CONFIRM:
        if ((psStackEvent->uEvent.sNwkLeaveConfirmEvent.eStatus == ZPS_E_SUCCESS) &&
            (psStackEvent->uEvent.sNwkLeaveConfirmEvent.u64ExtAddr == 0UL))
        {
            vFactoryResetAndRestart();
        }
        break;

    case ZPS_EVENT_NWK_DISCOVERY_COMPLETE:
    {
        tsBDB_ZCLEvent sBdbEvent;
        tsZCL_CallBackEvent sCallBackEvent;

        sCallBackEvent.pZPSevent = psStackEvent;
        sBdbEvent.eType = BDB_E_ZCL_EVENT_DISCOVERY_DONE;
        sBdbEvent.psCallBackEvent = &sCallBackEvent;
        BDB_vZclEventHandler(&sBdbEvent);
        break;
    }

    default:
        break;
    }
}

PRIVATE void vHandleAfEvent(BDB_tsZpsAfEvent *psZpsAfEvent)
{
    ZPS_tsAfEvent *psStackEvent = &psZpsAfEvent->sStackEvent;

    if (APP_ZCL_bIsRelayEndpoint(psZpsAfEvent->u8EndPoint))
    {
        if (psStackEvent->eType == ZPS_EVENT_APS_DATA_INDICATION)
        {
            APP_ZCL_vEventHandler(psStackEvent);
        }
    }
    else if (psZpsAfEvent->u8EndPoint == ZDO_ENDPOINT)
    {
        vHandleZdoEvent(psStackEvent);
    }

    if (psStackEvent->eType == ZPS_EVENT_APS_DATA_INDICATION)
    {
        PDUM_eAPduFreeAPduInstance(psStackEvent->uEvent.sApsDataIndEvent.hAPduInst);
    }
}

PUBLIC void APP_vInitialiseNode(void)
{
    BDB_tsInitArgs sInitArgs;
    uint64 u64Epid;

    ZPS_u32MacSetTxBuffers(4);
    ZPS_eAplAfInit();

    /* the stack restored its records from the PDM: there is a network if it has an extended PAN id */
    u64Epid = ZPS_u64NwkNibGetEpid(ZPS_pvAplZdoGetNwkHandle());
    DBG_vPrintf(TRACE_NODE, "NODE: saved network %s\n", u64Epid ? "present" : "none");

    APP_ZCL_vInitialise();

    eNodeState = u64Epid ? E_NFN_START : E_STARTUP;
    sBDB.sAttrib.bbdbNodeIsOnANetwork = u64Epid ? TRUE : FALSE;

    sInitArgs.hBdbEventsMsgQ = &APP_msgBdbEvents;
    BDB_vInit(&sInitArgs);
    sBDB.sAttrib.u32bdbPrimaryChannelSet = BDB_PRIMARY_CHANNEL_SET;
    sBDB.sAttrib.u32bdbSecondaryChannelSet = BDB_SECONDARY_CHANNEL_SET;
}

PUBLIC bool_t APP_bIsRunning(void)
{
    return eNodeState == E_RUNNING;
}

PUBLIC void APP_cbTimerSteering(void *pvParam)
{
    if (eNodeState != E_RUNNING)
    {
        vStartSteering();
    }
}

/* called by the BDB library */
PUBLIC void APP_vBdbCallback(BDB_tsBdbEvent *psBdbEvent)
{
    switch (psBdbEvent->eEventType)
    {
    case BDB_EVENT_ZPSAF:
        vHandleAfEvent(&psBdbEvent->uEventData.sZpsAfEvent);
        break;

    case BDB_EVENT_INIT_SUCCESS:
        if (eNodeState == E_STARTUP)
        {
            u8SteeringAttempts = 0;
            vStartSteering();
        }
        else
        {
            DBG_vPrintf(TRACE_NODE, "NODE: rejoined saved network\n");
            vSetRunning();
            APP_ZCL_vReportAll();
        }
        break;

    case BDB_EVENT_NWK_STEERING_SUCCESS:
        DBG_vPrintf(TRUE, "NODE: joined network\n");
        vSetRunning();
        LED_vSetMode(LED_E_JOINED);
        APP_ZCL_vReportAll();
        break;

    case BDB_EVENT_NO_NETWORK:
        DBG_vPrintf(TRACE_NODE, "NODE: no network found\n");
        if (eNodeState != E_RUNNING)
        {
            if (++u8SteeringAttempts < STEERING_MAX_ATTEMPTS)
            {
                ZTIMER_eStart(u8TimerSteering, STEERING_RETRY_TIME);
            }
            else
            {
                /* give up until the button is pressed */
                LED_vSetMode(LED_E_ERROR);
            }
        }
        break;

    default:
        break;
    }
}

PUBLIC void APP_vTaskNode(void)
{
    APP_tsEvent sEvent;

    if (!ZQ_bQueueReceive(&APP_msgAppEvents, &sEvent))
    {
        return;
    }

    switch (sEvent.eType)
    {
    case APP_E_EVENT_BUTTON_SHORT:
        if (eNodeState == E_RUNNING)
        {
            APP_ZCL_vToggleRelay(0);
        }
        else
        {
            u8SteeringAttempts = 0;
            vStartSteering();
        }
        break;

    case APP_E_EVENT_BUTTON_LONG:
        vLeaveAndReset();
        break;

    case APP_E_EVENT_SWITCH:
        APP_ZCL_vToggleRelay(sEvent.u8Channel);
        break;

    default:
        break;
    }
}
