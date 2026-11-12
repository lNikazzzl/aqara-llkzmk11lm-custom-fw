/*
 * PL7211 power meter probe, see pl7211.h. Read only: no write command is ever sent.
 *
 * Report 0xFFF5 (34 bytes): probe version, flags, CFG 0x3800..0x380F sampled on
 * the rising SCLK edge, the same 16 bytes sampled on the falling edge.
 * Report 0xFFF6 (42 bytes, rising edge), 6 bytes each, low byte first:
 * Vrms 0x3078 (/2^24 V), Irms 0x3084 (/2^30 A), active power 0x3090 (/2^24 W),
 * CF count 0x312C (x0.3125 Wh), ZccCnt 0x3018, ZccStart 0x301E, ZccStop 0x3024.
 * Addresses and scales from Prolific AN-71161001 (standard DSP code).
 */
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "dbg.h"
#include "hw.h"
#include "app_node.h"
#include "lumi.h"
#include "pl7211.h"

#ifdef DEBUG_LUMI
#define TRACE_PL TRUE
#else
#define TRACE_PL FALSE
#endif

#define PL_RSTN             DIO(3)
#define PL_CS               DIO(19)

#define PL_PROBE_VERSION    1
#define PL_CMD_READ         0x30        /* read without CRC */
#define PL_SPI_DIVIDER      8           /* 16 MHz / (2 * 8) = 1 MHz, chip max 2 MHz */

#define ATTR_PROBE_CFG      0xFFF5
#define ATTR_PROBE_DATA     0xFFF6

#define TICKS_RESET_LOW     2           /* 200 ms RSTN low, CS low */
#define TICKS_POS           5           /* 500 ms for the power on sequence (MTP -> RAM) */
#define TICKS_FIRST_DUMP    (40 * 10)
#define TICKS_DUMP_PERIOD   (60 * 10)

typedef enum
{
    E_PL_START,
    E_PL_RESET,
    E_PL_POS,
    E_PL_READY
} tePlState;

PRIVATE tePlState eState = E_PL_START;
PRIVATE uint16 u16Ticks;

PRIVATE void vSpiSetup(bool_t bRxNegEdge)
{
    /* no hardware slave select: CS is driven as a GPIO so the probe controls it at reset */
    vAHI_SpiConfigure(0, E_AHI_SPIM_MSB_FIRST, E_AHI_SPIM_TXNEG_EDGE, bRxNegEdge,
                      PL_SPI_DIVIDER, E_AHI_SPIM_INT_DISABLE, E_AHI_SPIM_AUTOSLAVE_DSABL);
}

PRIVATE uint8 u8Xfer(uint8 u8Out)
{
    vAHI_SpiStartTransfer(7, u8Out);
    vAHI_SpiWaitBusy();
    return u8AHI_SpiReadTransfer8();
}

PRIVATE void vRead(uint16 u16Addr, uint8 *pu8Out, uint8 u8Len)
{
    uint8 i;

    vAHI_DioSetOutput(0, PL_CS);
    u8Xfer(PL_CMD_READ | ((u8Len < 16) ? u8Len : 0));
    u8Xfer(u16Addr >> 8);
    u8Xfer(u16Addr & 0xff);
    for (i = 0; i < u8Len; i++)
    {
        pu8Out[i] = u8Xfer(0x00);
    }
    vAHI_DioSetOutput(PL_CS, 0);
}

PRIVATE void vDump(void)
{
    uint8 au8Buf[42];
    uint8 i;

    au8Buf[0] = PL_PROBE_VERSION;
    au8Buf[1] = 0;
    vSpiSetup(E_AHI_SPIM_RXNEG_EDGE);
    vRead(0x3800, &au8Buf[18], 16);
    vSpiSetup(E_AHI_SPIM_RXPOS_EDGE);
    vRead(0x3800, &au8Buf[2], 16);
    LUMI_vSendOctetReport(ATTR_PROBE_CFG, au8Buf, 34);
    DBG_vPrintf(TRACE_PL, "PL7211: cfg");
    for (i = 2; i < 34; i++)
    {
        DBG_vPrintf(TRACE_PL, " %02x", au8Buf[i]);
    }
    DBG_vPrintf(TRACE_PL, "\n");

    vRead(0x3078, &au8Buf[0], 6);
    vRead(0x3084, &au8Buf[6], 6);
    vRead(0x3090, &au8Buf[12], 6);
    vRead(0x312C, &au8Buf[18], 6);
    vRead(0x3018, &au8Buf[24], 18);     /* ZccCnt, ZccStart, ZccStop are contiguous */
    LUMI_vSendOctetReport(ATTR_PROBE_DATA, au8Buf, 42);
    DBG_vPrintf(TRACE_PL, "PL7211: data");
    for (i = 0; i < 42; i++)
    {
        DBG_vPrintf(TRACE_PL, " %02x", au8Buf[i]);
    }
    DBG_vPrintf(TRACE_PL, "\n");
}

PUBLIC void PL7211_vTick100ms(void)
{
    u16Ticks++;
    switch (eState)
    {
    case E_PL_START:
        /* reset with CS low: the chip latches SPI mode (MODE pin is pulled up) */
        vAHI_DioSetOutput(0, PL_RSTN | PL_CS);
        vAHI_DioSetDirection(0, PL_RSTN | PL_CS);
        u16Ticks = 0;
        eState = E_PL_RESET;
        break;

    case E_PL_RESET:
        if (u16Ticks >= TICKS_RESET_LOW)
        {
            vAHI_DioSetOutput(PL_RSTN, 0);
            u16Ticks = 0;
            eState = E_PL_POS;
        }
        break;

    case E_PL_POS:
        if (u16Ticks >= TICKS_POS)
        {
            vAHI_DioSetOutput(PL_CS, 0);
            vSpiSetup(E_AHI_SPIM_RXPOS_EDGE);
            DBG_vPrintf(TRACE_PL, "PL7211: reset done\n");
            u16Ticks = TICKS_DUMP_PERIOD - TICKS_FIRST_DUMP;
            eState = E_PL_READY;
        }
        break;

    case E_PL_READY:
        if (u16Ticks >= TICKS_DUMP_PERIOD)
        {
            u16Ticks = 0;
            if (APP_bIsRunning())
            {
                vDump();
            }
        }
        break;
    }
}
