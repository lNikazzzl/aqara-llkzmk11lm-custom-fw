/*
 * Raw EEPROM storage for this firmware's settings, see app_store.h.
 *
 * Each save goes to the next of STORE_SLOTS segments with an increasing
 * sequence number and a CRC; loading picks the valid slot with the highest
 * sequence number.
 */
#include <jendefs.h>
#include <string.h>
#include "AppHardwareApi.h"
#include "dbg.h"
#include "zcl_options.h"
#include "OnOff.h"
#include "app_store.h"

#ifdef DEBUG_STORE
#define TRACE_STORE TRUE
#else
#define TRACE_STORE FALSE
#endif

#define STORE_MAGIC     0x52454C59UL    /* "RELY" */

typedef struct
{
    uint32 u32Magic;
    uint32 u32Seq;
    tsAppData sData;
    uint32 u32Crc;
} tsSlot;

PRIVATE uint32 u32NextSeq;
PRIVATE uint8 u8NextSlot;

PRIVATE uint32 u32Crc32(const uint8 *pu8Data, uint16 u16Len)
{
    uint32 u32Crc = 0xFFFFFFFFUL;
    uint8 i;

    while (u16Len--)
    {
        u32Crc ^= *pu8Data++;
        for (i = 0; i < 8; i++)
        {
            u32Crc = (u32Crc >> 1) ^ (0xEDB88320UL & (0UL - (u32Crc & 1)));
        }
    }
    return ~u32Crc;
}

PRIVATE void vDefaults(tsAppData *psData)
{
    uint8 i;

    memset(psData, 0, sizeof(*psData));
    for (i = 0; i < RELAY_CHANNELS; i++)
    {
        psData->au8StartUp[i] = CLD_ONOFF_START_MODE;
    }
    psData->bOutageMemory = TRUE;
}

PUBLIC void STORE_vLoad(tsAppData *psData)
{
    tsSlot sSlot;
    uint8 u8SegLen;
    uint8 i;
    bool_t bFound = FALSE;
    uint32 u32BestSeq = 0;

    vDefaults(psData);
    u32NextSeq = 1;
    u8NextSlot = 0;

    (void)u16AHI_InitialiseEEP(&u8SegLen);
    if (u8SegLen < sizeof(tsSlot))
    {
        DBG_vPrintf(TRUE, "STORE: segment too small (%d)\n", u8SegLen);
        return;
    }

    for (i = 0; i < STORE_SLOTS; i++)
    {
        if ((iAHI_ReadDataFromEEPROMsegment(STORE_FIRST_SEGMENT + i, 0, (uint8 *)&sSlot, sizeof(sSlot)) != 0) ||
            (sSlot.u32Magic != STORE_MAGIC) ||
            (sSlot.u32Crc != u32Crc32((uint8 *)&sSlot, sizeof(sSlot) - sizeof(uint32))))
        {
            continue;
        }
        if (!bFound || (sSlot.u32Seq > u32BestSeq))
        {
            bFound = TRUE;
            u32BestSeq = sSlot.u32Seq;
            *psData = sSlot.sData;
            u32NextSeq = sSlot.u32Seq + 1;
            u8NextSlot = (i + 1) % STORE_SLOTS;
        }
    }
    DBG_vPrintf(TRACE_STORE, "STORE: %s, seq %d\n", bFound ? "loaded" : "defaults", u32BestSeq);
}

PUBLIC void STORE_vSave(const tsAppData *psData)
{
    tsSlot sSlot;
    uint8 u8Segment = STORE_FIRST_SEGMENT + u8NextSlot;

    sSlot.u32Magic = STORE_MAGIC;
    sSlot.u32Seq = u32NextSeq;
    sSlot.sData = *psData;
    sSlot.u32Crc = u32Crc32((uint8 *)&sSlot, sizeof(sSlot) - sizeof(uint32));

    if ((iAHI_EraseEEPROMsegment(u8Segment) != 0) ||
        (iAHI_WriteDataIntoEEPROMsegment(u8Segment, 0, (uint8 *)&sSlot, sizeof(sSlot)) != 0))
    {
        DBG_vPrintf(TRUE, "STORE: write to segment %d failed\n", u8Segment);
        return;
    }
    DBG_vPrintf(TRACE_STORE, "STORE: saved seq %d to segment %d\n", u32NextSeq, u8Segment);
    u32NextSeq++;
    u8NextSlot = (u8NextSlot + 1) % STORE_SLOTS;
}
