/*
 * Raw EEPROM access to the PDM area for the firmware hand-over, see nvm_clean.h.
 */
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "dbg.h"
#include "nvm_clean.h"

/* the factory PDM area; segment 127 holds non-PDM factory data */
#define NVM_PDM_LAST_SEGMENT        126
#define NVM_SEGMENT_LEN             64

PRIVATE uint16 u16SegmentId(uint8 u8Segment)
{
    uint8 au8Hdr[2];

    if (iAHI_ReadDataFromEEPROMsegment(u8Segment, 0, au8Hdr, sizeof(au8Hdr)) != 0)
    {
        return 0;
    }
    return ((uint16)au8Hdr[0] << 8) | au8Hdr[1];
}

PRIVATE bool_t bIsStackOrOurs(uint16 u16Id)
{
    return ((u16Id >= 0xF000) && (u16Id <= 0xF1FF)) ||
           ((u16Id >= 0xA100) && (u16Id <= 0xA1FF));
}

PRIVATE uint32 u32Crc32(uint32 u32Crc, const uint8 *pu8Data, uint8 u8Len)
{
    uint8 i;

    while (u8Len--)
    {
        u32Crc ^= *pu8Data++;
        for (i = 0; i < 8; i++)
        {
            u32Crc = (u32Crc >> 1) ^ (0xEDB88320UL & (0UL - (u32Crc & 1)));
        }
    }
    return u32Crc;
}

PUBLIC uint32 NVM_u32FactoryFingerprint(void)
{
    uint8 au8Seg[NVM_SEGMENT_LEN];
    uint32 u32Crc = 0xFFFFFFFFUL;
    uint8 u8SegLen;
    uint8 s;

    (void)u16AHI_InitialiseEEP(&u8SegLen);
    for (s = 0; s <= NVM_PDM_LAST_SEGMENT; s++)
    {
        uint16 u16Id = u16SegmentId(s);

        /* Lumi records only; a blank segment reads as id 0 */
        if ((u16Id == 0) || (u16Id >= 0x0100) ||
            (iAHI_ReadDataFromEEPROMsegment(s, 0, au8Seg, sizeof(au8Seg)) != 0))
        {
            continue;
        }
        u32Crc = u32Crc32(u32Crc, &s, 1);
        u32Crc = u32Crc32(u32Crc, au8Seg, sizeof(au8Seg));
    }
    return ~u32Crc;
}

PUBLIC uint8 NVM_u8EraseStackRecords(void)
{
    uint8 u8SegLen;
    uint8 u8Erased = 0;
    uint8 s;

    (void)u16AHI_InitialiseEEP(&u8SegLen);
    for (s = 0; s <= NVM_PDM_LAST_SEGMENT; s++)
    {
        if (bIsStackOrOurs(u16SegmentId(s)) && (iAHI_EraseEEPROMsegment(s) == 0))
        {
            u8Erased++;
        }
    }
    DBG_vPrintf(TRUE, "NVM: erased %d stack segments\n", u8Erased);
    return u8Erased;
}
