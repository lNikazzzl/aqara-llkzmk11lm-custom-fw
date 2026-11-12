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
 * ZCL side: relay 1 on EP1, relay 2 on EP2, presented like the factory firmware.
 *
 * The OnOff attribute of each endpoint is the source of truth. Whenever it
 * changes (remote command, timed off, local button or wall switch) the relay
 * is pulsed to match and the new value is reported to the coordinator.
 */
#include <jendefs.h>
#include <string.h>
#include "dbg.h"
#include "pdum_gen.h"
#include "zps_apl_af.h"
#include "ZTimer.h"
#include "zcl.h"
#include "zcl_options.h"

#include "hw.h"
#include "app_main.h"
#include "app_node.h"
#include "app_ota.h"
#include "app_store.h"
#include "app_zcl.h"
#include "led.h"
#include "lumi.h"
#include "pl7211.h"
#include "relay.h"

#ifdef DEBUG_ZCL
#define TRACE_ZCL TRUE
#else
#define TRACE_ZCL FALSE
#endif

#define ZCL_TICK_MS             100
#define TICKS_PER_SECOND        (1000 / ZCL_TICK_MS)
#define SAVE_DELAY_TICKS        (2 * TICKS_PER_SECOND)
#define HEARTBEAT_PERIOD_SEC    (50 * 60)
#define HEARTBEAT_FIRST_SEC     30
#define COORDINATOR_ADDR        0x0000
#define COORDINATOR_EP          1

PRIVATE tsAppData sData;
PRIVATE bool_t abApplied[RELAY_CHANNELS];
PRIVATE uint8 u8SaveCountdown;
PRIVATE uint8 u8Tick;
PRIVATE uint32 u32HeartbeatSec = HEARTBEAT_FIRST_SEC;

PRIVATE void APP_ZCL_cbGeneralCallback(tsZCL_CallBackEvent *psEvent);
PRIVATE void APP_ZCL_cbEndpointCallback(tsZCL_CallBackEvent *psEvent);

/* Save soon rather than on every change to spare the EEPROM */
PRIVATE void vScheduleSave(void)
{
    u8SaveCountdown = SAVE_DELAY_TICKS;
}

PRIVATE void vSaveNow(void)
{
    uint8 i;

    for (i = 0; i < RELAY_CHANNELS; i++)
    {
        sData.au8OnOff[i] = DEV_psOnOff(i)->bOnOff;
        sData.au8StartUp[i] = DEV_psOnOff(i)->eStartUpOnOff;
    }
    STORE_vSave(&sData);
}

PRIVATE void vReport(uint8 u8Ch)
{
    PDUM_thAPduInstance hAPduInst;
    tsZCL_Address sAddr;
    teZCL_Status eStatus;

    if (!APP_bIsRunning())
    {
        return;
    }

    hAPduInst = PDUM_hAPduAllocateAPduInstance(apduZCL);
    if (hAPduInst == PDUM_INVALID_HANDLE)
    {
        return;
    }

    sAddr.eAddressMode = E_ZCL_AM_SHORT;
    sAddr.uAddress.u16DestinationAddress = COORDINATOR_ADDR;

    eStatus = eZCL_ReportAttribute(&sAddr, GENERAL_CLUSTER_ID_ONOFF, E_CLD_ONOFF_ATTR_ID_ONOFF,
                                   RELAY_EP_FIRST + u8Ch, COORDINATOR_EP, hAPduInst);
    if ((eStatus == E_ZCL_ERR_EP_RANGE) || (eStatus == E_ZCL_ERR_EP_UNKNOWN) ||
        (eStatus == E_ZCL_ERR_CLUSTER_NOT_FOUND) || (eStatus == E_ZCL_ERR_ATTRIBUTE_NOT_FOUND) ||
        (eStatus == E_ZCL_ERR_ATTRIBUTE_NOT_REPORTABLE))
    {
        /* these fail before the frame is handed to the stack, so the buffer is still ours */
        PDUM_eAPduFreeAPduInstance(hAPduInst);
    }
    DBG_vPrintf(TRACE_ZCL, "ZCL: report ch%d status %d\n", u8Ch + 1, eStatus);
}

PRIVATE void vHeartbeat(void)
{
    if (APP_bIsRunning())
    {
        LUMI_vSendHeartbeat(sData.u16PowerOnCount, DEV_psOnOff(0)->bOnOff, DEV_psOnOff(1)->bOnOff);
    }
}

/* Make the relay follow the OnOff attribute */
PRIVATE void vSyncRelay(uint8 u8Ch)
{
    bool_t bOn = DEV_psOnOff(u8Ch)->bOnOff ? TRUE : FALSE;
    uint8 u8Other = u8Ch ^ 1;

    if (bOn == abApplied[u8Ch])
    {
        return;
    }

    /* interlock: never both on, switch the other one off first */
    if (bOn && sData.bInterlock && DEV_psOnOff(u8Other)->bOnOff)
    {
        DEV_psOnOff(u8Other)->bOnOff = FALSE;
        abApplied[u8Other] = FALSE;
        RELAY_vSet(u8Other, FALSE);
        vReport(u8Other);
    }

    abApplied[u8Ch] = bOn;
    RELAY_vSet(u8Ch, bOn);
    vReport(u8Ch);
    vScheduleSave();
}

PRIVATE void vSetInterlock(bool_t bOn)
{
    sData.bInterlock = bOn;
    if (bOn && DEV_psOnOff(0)->bOnOff && DEV_psOnOff(1)->bOnOff)
    {
        DEV_psOnOff(1)->bOnOff = FALSE;
        vSyncRelay(1);
    }
    vScheduleSave();
}

PRIVATE void vSetOutageMemory(bool_t bOn)
{
    uint8 i;

    /* power outage memory is StartUpOnOff "previous" vs "off" */
    sData.bOutageMemory = bOn;
    for (i = 0; i < RELAY_CHANNELS; i++)
    {
        DEV_psOnOff(i)->eStartUpOnOff = bOn ? E_CLD_ONOFF_STARTUPONOFF_PREVIOUS_VALUE : E_CLD_ONOFF_STARTUPONOFF_OFF;
    }
    vScheduleSave();
}

PRIVATE void vInitBasic(void)
{
    tsCLD_Basic *psBasic = &sEp1.sBasicServerCluster;

    memcpy(psBasic->au8ManufacturerName, BAS_MANUF_NAME_STRING, CLD_BAS_MANUF_NAME_SIZE);
    memcpy(psBasic->au8ModelIdentifier, BAS_MODEL_ID_STRING, CLD_BAS_MODEL_ID_SIZE);
    memcpy(psBasic->au8DateCode, BAS_DATE_STRING, CLD_BAS_DATE_SIZE);
    memcpy(psBasic->au8SWBuildID, BAS_SW_BUILD_STRING, CLD_BAS_SW_BUILD_SIZE);
}

PRIVATE void vRegisterEndpoints(void)
{
    teZCL_Status eStatus = DEV_eRegisterEndpoints(APP_ZCL_cbEndpointCallback);

    if (eStatus != E_ZCL_SUCCESS)
    {
        DBG_vPrintf(TRUE, "ZCL: endpoint registration failed %d\n", eStatus);
    }
    vInitBasic();
}

/* Apply StartUpOnOff to the saved state */
PRIVATE void vRestoreState(void)
{
    uint8 i;

    for (i = 0; i < RELAY_CHANNELS; i++)
    {
        tsCLD_OnOff *psOnOff = DEV_psOnOff(i);
        bool_t bOn;

        switch (sData.au8StartUp[i])
        {
        case E_CLD_ONOFF_STARTUPONOFF_OFF:      bOn = FALSE;                    break;
        case E_CLD_ONOFF_STARTUPONOFF_ON:       bOn = TRUE;                     break;
        case E_CLD_ONOFF_STARTUPONOFF_TOGGLE:   bOn = !sData.au8OnOff[i];       break;
        default:                                bOn = sData.au8OnOff[i];        break;
        }
        if ((i == 1) && sData.bInterlock && DEV_psOnOff(0)->bOnOff)
        {
            bOn = FALSE;
        }

        psOnOff->bOnOff = bOn;
        psOnOff->eStartUpOnOff = sData.au8StartUp[i];
        DBG_vPrintf(TRACE_ZCL, "ZCL: ch%d start %s\n", i + 1, bOn ? "on" : "off");

        /* the latching relay may be in either position after a power cut, always pulse */
        abApplied[i] = bOn;
        RELAY_vSet(i, bOn);
    }
}

PUBLIC void APP_ZCL_vInitialise(void)
{
    static const tsLumiCallbacks sLumiCallbacks = { vSetInterlock, vSetOutageMemory };
    teZCL_Status eStatus;

    STORE_vLoad(&sData);
    sData.u16PowerOnCount++;
    STORE_vSave(&sData);
    DBG_vPrintf(TRACE_ZCL, "ZCL: power on #%d, interlock %d, outage memory %d\n",
                sData.u16PowerOnCount, sData.bInterlock, sData.bOutageMemory);

    eStatus = eZCL_Initialise(APP_ZCL_cbGeneralCallback, apduZCL);
    if (eStatus != E_ZCL_SUCCESS)
    {
        DBG_vPrintf(TRUE, "ZCL: init failed %d\n", eStatus);
    }

    vRegisterEndpoints();
    vRestoreState();
    LUMI_vInit(&sLumiCallbacks);
    APP_OTA_vInit();

    ZTIMER_eStart(u8TimerZclTick, ZTIMER_TIME_MSEC(ZCL_TICK_MS));
}

PUBLIC bool_t APP_ZCL_bIsRelayEndpoint(uint8 u8Endpoint)
{
    return (u8Endpoint >= RELAY_EP_FIRST) && (u8Endpoint < RELAY_EP_FIRST + RELAY_CHANNELS);
}

PUBLIC void APP_ZCL_vEventHandler(ZPS_tsAfEvent *psStackEvent)
{
    tsZCL_CallBackEvent sCallBackEvent;

    if ((psStackEvent->eType == ZPS_EVENT_APS_DATA_INDICATION) &&
        LUMI_bHandleDataIndication(&psStackEvent->uEvent.sApsDataIndEvent))
    {
        return;
    }

    sCallBackEvent.pZPSevent = psStackEvent;
    sCallBackEvent.eEventType = E_ZCL_CBET_ZIGBEE_EVENT;
    vZCL_EventHandler(&sCallBackEvent);
}

PUBLIC void APP_ZCL_cbTimerTick(void *pvParam)
{
    ZTIMER_eStart(u8TimerZclTick, ZTIMER_TIME_MSEC(ZCL_TICK_MS));

    eZCL_Update100mS();
    PL7211_vTick100ms();

    if (++u8Tick >= TICKS_PER_SECOND)
    {
        tsZCL_CallBackEvent sCallBackEvent;

        u8Tick = 0;
        sCallBackEvent.pZPSevent = NULL;
        sCallBackEvent.eEventType = E_ZCL_CBET_TIMER;
        vZCL_EventHandler(&sCallBackEvent);

        APP_OTA_vTick1s();

        if (--u32HeartbeatSec == 0)
        {
            u32HeartbeatSec = HEARTBEAT_PERIOD_SEC;
            vHeartbeat();
        }
    }

    if (u8SaveCountdown && (--u8SaveCountdown == 0))
    {
        vSaveNow();
    }
}

PUBLIC void APP_ZCL_vToggleRelay(uint8 u8Ch)
{
    if (u8Ch >= RELAY_CHANNELS)
    {
        return;
    }
    DEV_psOnOff(u8Ch)->bOnOff = !DEV_psOnOff(u8Ch)->bOnOff;
    vSyncRelay(u8Ch);
}

PUBLIC void APP_ZCL_vReportAll(void)
{
    uint8 i;

    for (i = 0; i < RELAY_CHANNELS; i++)
    {
        vReport(i);
    }
    vHeartbeat();
}

PRIVATE void APP_ZCL_cbGeneralCallback(tsZCL_CallBackEvent *psEvent)
{
    if (psEvent->eEventType == E_ZCL_CBET_ERROR)
    {
        DBG_vPrintf(TRACE_ZCL, "ZCL: error %d\n", psEvent->eZCL_Status);
    }
}

PRIVATE void vUpdateIdentify(void)
{
    bool_t bIdentify = sEp1.sIdentifyServerCluster.u16IdentifyTime != 0;

    if (bIdentify && (LED_eGetMode() != LED_E_IDENTIFY))
    {
        LED_vSetMode(LED_E_IDENTIFY);
    }
    else if (!bIdentify && (LED_eGetMode() == LED_E_IDENTIFY))
    {
        LED_vSetMode(LED_E_OFF);
    }
}

/* 10 Hz identify tick from the ZCL (CLD_IDENTIFY_10HZ_TICK); the LED module blinks on its own */
PUBLIC void vIdEffectTick(uint8 u8Endpoint)
{
}

PRIVATE void vHandleFactoryDefaults(void)
{
    uint8 i;

    /* resets attributes only, the device stays on the network */
    DBG_vPrintf(TRACE_ZCL, "ZCL: reset to factory defaults\n");
    DEV_vResetStructures();
    vRegisterEndpoints();
    APP_OTA_vInit();
    sData.bInterlock = FALSE;
    sData.bOutageMemory = TRUE;
    for (i = 0; i < RELAY_CHANNELS; i++)
    {
        DEV_psOnOff(i)->eStartUpOnOff = CLD_ONOFF_START_MODE;
        DEV_psOnOff(i)->bOnOff = abApplied[i];
    }
    vScheduleSave();
}

PRIVATE void APP_ZCL_cbEndpointCallback(tsZCL_CallBackEvent *psEvent)
{
    uint8 u8Ch = psEvent->u8EndPoint - RELAY_EP_FIRST;

    if (u8Ch >= RELAY_CHANNELS)
    {
        return;
    }

    switch (psEvent->eEventType)
    {
    case E_ZCL_CBET_CLUSTER_CUSTOM:
        switch (psEvent->uMessage.sClusterCustomMessage.u16ClusterId)
        {
        case GENERAL_CLUSTER_ID_ONOFF:
            vSyncRelay(u8Ch);
            break;

        case GENERAL_CLUSTER_ID_IDENTIFY:
            vUpdateIdentify();
            break;

        case GENERAL_CLUSTER_ID_BASIC:
        {
            tsCLD_BasicCallBackMessage *psMsg =
                (tsCLD_BasicCallBackMessage *)psEvent->uMessage.sClusterCustomMessage.pvCustomData;

            if (psMsg->u8CommandId == E_CLD_BASIC_CMD_RESET_TO_FACTORY_DEFAULTS)
            {
                vHandleFactoryDefaults();
            }
            break;
        }

        case OTA_CLUSTER_ID:
            APP_OTA_vHandleEvent((tsOTA_CallBackMessage *)psEvent->uMessage.sClusterCustomMessage.pvCustomData);
            break;

        default:
            break;
        }
        break;

    case E_ZCL_CBET_CLUSTER_UPDATE:
        /* local changes, e.g. the On With Timed Off timer expiring */
        if (psEvent->psClusterInstance->psClusterDefinition->u16ClusterEnum == GENERAL_CLUSTER_ID_ONOFF)
        {
            vSyncRelay(u8Ch);
        }
        else if (psEvent->psClusterInstance->psClusterDefinition->u16ClusterEnum == GENERAL_CLUSTER_ID_IDENTIFY)
        {
            vUpdateIdentify();
        }
        break;

    case E_ZCL_CBET_WRITE_INDIVIDUAL_ATTRIBUTE:
        if ((psEvent->eZCL_Status == E_ZCL_SUCCESS) &&
            (psEvent->psClusterInstance->psClusterDefinition->u16ClusterEnum == GENERAL_CLUSTER_ID_ONOFF) &&
            (psEvent->uMessage.sIndividualAttributeResponse.u16AttributeEnum == E_CLD_ONOFF_ATTR_ID_STARTUP_ONOFF))
        {
            vScheduleSave();
        }
        break;

    default:
        break;
    }
}
