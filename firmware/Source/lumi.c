/*
 * Lumi manufacturer specific frames, see lumi.h.
 *
 * They are handled on the raw APS frame before it reaches the ZCL, because
 * the attributes do not exist in the SDK clusters.
 */
#include <jendefs.h>
#include <string.h>
#include "AppHardwareApi.h"
#include "dbg.h"
#include "pdum_apl.h"
#include "pdum_gen.h"
#include "zps_apl_af.h"
#include "zcl_customcommand.h"
#include "lumi.h"

#ifdef DEBUG_LUMI
#define TRACE_LUMI TRUE
#else
#define TRACE_LUMI FALSE
#endif

#define CLUSTER_BASIC               0x0000
#define CLUSTER_BINARY_OUTPUT       0x0010

#define ATTR_BASIC_HEARTBEAT        0xFF01
#define ATTR_BASIC_OUTAGE_MEMORY    0xFFF0
#define ATTR_BINOUT_INTERLOCK       0xFF06

#define ZCL_FC_MANUF_SPECIFIC       0x04
#define ZCL_FC_SERVER_TO_CLIENT     0x08
#define ZCL_FC_DISABLE_DEFAULT_RSP  0x10

#define ZCL_CMD_WRITE               0x02
#define ZCL_CMD_WRITE_UNDIVIDED     0x03
#define ZCL_CMD_WRITE_RSP           0x04
#define ZCL_CMD_WRITE_NO_RSP        0x05
#define ZCL_CMD_REPORT              0x0A

#define ZCL_STATUS_SUCCESS          0x00
#define ZCL_STATUS_UNSUPPORTED_ATTR 0x86
#define ZCL_STATUS_INVALID_VALUE    0x87

#define ZCL_TYPE_BOOL               0x10
#define ZCL_TYPE_UINT16             0x21
#define ZCL_TYPE_INT8               0x28
#define ZCL_TYPE_OCTSTR             0x41
#define ZCL_TYPE_CHARSTR            0x42

#define COORDINATOR_ADDR            0x0000
#define COORDINATOR_EP              1
#define LUMI_EP                     1

PRIVATE tsLumiCallbacks sCallbacks;
PRIVATE bool_t bAdcReady;

/* Size of a ZCL value of the given type at pu8, or 0 if the type is not supported */
PRIVATE uint16 u16ValueSize(uint8 u8Type, const uint8 *pu8, uint16 u16Left)
{
    switch (u8Type)
    {
    case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30:
        return 1;
    case 0x09: case 0x19: case 0x21: case 0x29: case 0x31:
        return 2;
    case 0x0B: case 0x1B: case 0x23: case 0x2B: case 0x39:
        return 4;
    case ZCL_TYPE_OCTSTR: case ZCL_TYPE_CHARSTR:
        return u16Left ? (uint16)(1 + pu8[0]) : 0;
    default:
        return 0;
    }
}

PRIVATE void vSend(uint16 u16Cluster, uint8 u8SrcEp, uint8 u8DstEp, uint16 u16DstAddr,
                   const uint8 *pu8Frame, uint16 u16Len)
{
    PDUM_thAPduInstance hAPdu = PDUM_hAPduAllocateAPduInstance(apduZCL);
    uint8 u8Seq;

    if (hAPdu == PDUM_INVALID_HANDLE)
    {
        return;
    }
    memcpy(PDUM_pvAPduInstanceGetPayload(hAPdu), pu8Frame, u16Len);
    PDUM_eAPduInstanceSetPayloadSize(hAPdu, u16Len);

    if (ZPS_eAplAfUnicastDataReq(hAPdu, u16Cluster, u8SrcEp, u8DstEp, u16DstAddr,
                                 ZPS_E_APL_AF_SECURE_NWK, 0, &u8Seq) != ZPS_E_SUCCESS)
    {
        PDUM_eAPduFreeAPduInstance(hAPdu);
    }
}

/* Power outage memory: [aa 80 05 d1 47 07|09 01 10 vv] carries the value, the second frame is an ack */
PRIVATE uint8 u8WriteOutageMemory(const uint8 *pu8Value, uint16 u16Size)
{
    uint8 u8Len = pu8Value[0];
    const uint8 *p = pu8Value + 1;

    if ((u16Size < 1 + 9) || (u8Len < 9))
    {
        return ZCL_STATUS_SUCCESS;
    }
    if ((p[0] == 0xaa) && (p[3] == 0xd1) && (p[7] == ZCL_TYPE_BOOL))
    {
        DBG_vPrintf(TRACE_LUMI, "LUMI: power outage memory %d\n", p[8]);
        if (sCallbacks.pfSetOutageMemory)
        {
            sCallbacks.pfSetOutageMemory(p[8] ? TRUE : FALSE);
        }
    }
    return ZCL_STATUS_SUCCESS;
}

PRIVATE uint8 u8WriteAttribute(uint16 u16Cluster, uint16 u16Attr, uint8 u8Type,
                               const uint8 *pu8Value, uint16 u16Size)
{
    if ((u16Cluster == CLUSTER_BASIC) && (u16Attr == ATTR_BASIC_OUTAGE_MEMORY) && (u8Type == ZCL_TYPE_OCTSTR))
    {
        return u8WriteOutageMemory(pu8Value, u16Size);
    }
    if ((u16Cluster == CLUSTER_BINARY_OUTPUT) && (u16Attr == ATTR_BINOUT_INTERLOCK) && (u8Type == ZCL_TYPE_BOOL))
    {
        DBG_vPrintf(TRACE_LUMI, "LUMI: interlock %d\n", pu8Value[0]);
        if (sCallbacks.pfSetInterlock)
        {
            sCallbacks.pfSetInterlock(pu8Value[0] ? TRUE : FALSE);
        }
        return ZCL_STATUS_SUCCESS;
    }
    return ZCL_STATUS_UNSUPPORTED_ATTR;
}

PUBLIC void LUMI_vInit(const tsLumiCallbacks *psCallbacks)
{
    sCallbacks = *psCallbacks;
}

PUBLIC bool_t LUMI_bHandleDataIndication(ZPS_tsAfDataIndEvent *psInd)
{
    const uint8 *p = PDUM_pvAPduInstanceGetPayload(psInd->hAPduInst);
    uint16 u16Len = PDUM_u16APduInstanceGetPayloadSize(psInd->hAPduInst);
    uint8 au8Rsp[5 + 3 * 8];
    uint16 u16RspLen = 5;
    uint16 u16Pos = 5;
    uint8 u8Cmd;
    bool_t bAllOk = TRUE;

    if ((u16Len < 5) || !(p[0] & ZCL_FC_MANUF_SPECIFIC) || ((p[0] & 0x03) != 0) ||
        ((p[1] | (p[2] << 8)) != LUMI_MANUFACTURER_CODE))
    {
        return FALSE;
    }
    if ((psInd->u16ClusterId != CLUSTER_BASIC) && (psInd->u16ClusterId != CLUSTER_BINARY_OUTPUT))
    {
        return FALSE;
    }
    u8Cmd = p[4];
    if ((u8Cmd != ZCL_CMD_WRITE) && (u8Cmd != ZCL_CMD_WRITE_UNDIVIDED) && (u8Cmd != ZCL_CMD_WRITE_NO_RSP))
    {
        return FALSE;
    }

    au8Rsp[0] = ZCL_FC_MANUF_SPECIFIC | ZCL_FC_SERVER_TO_CLIENT | ZCL_FC_DISABLE_DEFAULT_RSP;
    au8Rsp[1] = p[1];
    au8Rsp[2] = p[2];
    au8Rsp[3] = p[3];
    au8Rsp[4] = ZCL_CMD_WRITE_RSP;

    while (u16Pos + 3 <= u16Len)
    {
        uint16 u16Attr = p[u16Pos] | (p[u16Pos + 1] << 8);
        uint8 u8Type = p[u16Pos + 2];
        uint16 u16Size = u16ValueSize(u8Type, &p[u16Pos + 3], u16Len - u16Pos - 3);
        uint8 u8Status;

        u16Pos += 3;
        if ((u16Size == 0) || (u16Pos + u16Size > u16Len))
        {
            break;
        }
        u8Status = u8WriteAttribute(psInd->u16ClusterId, u16Attr, u8Type, &p[u16Pos], u16Size);
        u16Pos += u16Size;

        if ((u8Status != ZCL_STATUS_SUCCESS) && (u16RspLen + 3 <= sizeof(au8Rsp)))
        {
            bAllOk = FALSE;
            au8Rsp[u16RspLen++] = u8Status;
            au8Rsp[u16RspLen++] = u16Attr & 0xff;
            au8Rsp[u16RspLen++] = u16Attr >> 8;
        }
    }

    if (u8Cmd != ZCL_CMD_WRITE_NO_RSP)
    {
        if (bAllOk)
        {
            au8Rsp[u16RspLen++] = ZCL_STATUS_SUCCESS;
        }
        vSend(psInd->u16ClusterId, psInd->u8DstEndpoint, psInd->u8SrcEndpoint,
              psInd->uSrcAddress.u16Addr, au8Rsp, u16RspLen);
    }
    return TRUE;
}

PUBLIC void LUMI_vSendHeartbeat(uint16 u16PowerOnCount, bool_t bRelay1, bool_t bRelay2)
{
    uint8 au8Frame[32];
    uint8 n = 0;
    uint8 u8LenPos;

    au8Frame[n++] = ZCL_FC_MANUF_SPECIFIC | ZCL_FC_SERVER_TO_CLIENT | ZCL_FC_DISABLE_DEFAULT_RSP;
    au8Frame[n++] = LUMI_MANUFACTURER_CODE & 0xff;
    au8Frame[n++] = LUMI_MANUFACTURER_CODE >> 8;
    au8Frame[n++] = u8GetTransactionSequenceNumber();
    au8Frame[n++] = ZCL_CMD_REPORT;
    au8Frame[n++] = ATTR_BASIC_HEARTBEAT & 0xff;
    au8Frame[n++] = ATTR_BASIC_HEARTBEAT >> 8;
    au8Frame[n++] = ZCL_TYPE_CHARSTR;
    u8LenPos = n++;

    /* tag, ZCL type, value */
    au8Frame[n++] = 3;   au8Frame[n++] = ZCL_TYPE_INT8;   au8Frame[n++] = (uint8)LUMI_i8ChipTemperature();
    au8Frame[n++] = 5;   au8Frame[n++] = ZCL_TYPE_UINT16; au8Frame[n++] = u16PowerOnCount & 0xff;
                                                          au8Frame[n++] = u16PowerOnCount >> 8;
    au8Frame[n++] = 100; au8Frame[n++] = ZCL_TYPE_BOOL;   au8Frame[n++] = bRelay1 ? 1 : 0;
    au8Frame[n++] = 101; au8Frame[n++] = ZCL_TYPE_BOOL;   au8Frame[n++] = bRelay2 ? 1 : 0;

    au8Frame[u8LenPos] = n - u8LenPos - 1;
    DBG_vPrintf(TRACE_LUMI, "LUMI: heartbeat, power on count %d\n", u16PowerOnCount);
    vSend(CLUSTER_BASIC, LUMI_EP, COORDINATOR_EP, COORDINATOR_ADDR, au8Frame, n);
}

/* Internal temperature sensor; conversion from the NXP JN-AN-1218 example */
PUBLIC int8 LUMI_i8ChipTemperature(void)
{
    uint16 u16Adc;
    int32 i32Temp;

    if (!bAdcReady)
    {
        vAHI_ApConfigure(E_AHI_AP_REGULATOR_ENABLE, E_AHI_AP_INT_DISABLE, E_AHI_AP_SAMPLE_8,
                         E_AHI_AP_CLOCKDIV_500KHZ, E_AHI_AP_INTREF);
        while (!bAHI_APRegulatorEnabled());
        bAdcReady = TRUE;
    }

    vAHI_AdcEnable(E_AHI_ADC_SINGLE_SHOT, E_AHI_AP_INPUT_RANGE_2, E_AHI_ADC_SRC_TEMP);
    vAHI_AdcStartSample();
    while (bAHI_AdcPoll());
    u16Adc = u16AHI_AdcRead();

    i32Temp = 25 - ((((int32)(u16Adc * 4) - 1210) * 353) / 1000);
    return (int8)i32Temp;
}
