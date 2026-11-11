/*
 * EEPROM dumper for JN5169: reads every EEPROM segment and prints it on UART0
 * at 115200 baud, repeating every few seconds.
 *
 * Built with EEPROM_IMAGE (make RESTORE=1) it also carries a full EEPROM image
 * and writes it back when it receives the line "RESTORE-EEPROM" on UART0.
 * Without that command it never writes or erases anything.
 *
 * Output:
 *   EEP <segments> <segment length>
 *   S<index>:<hex bytes>          one line per segment
 *   END <crc32 of all bytes>
 */
#include <jendefs.h>
#include <AppHardwareApi.h>

#define UART            E_AHI_UART_0
#define LED_BLUE        (1UL << 1)
#define MAX_SEG_LEN     64
#define CR              '\r'
#define LF              '\n'

#ifdef EEPROM_IMAGE
#include "eeprom_image.h"   /* generated from eeprom_backup.bin */
#define RESTORE_COMMAND "RESTORE-EEPROM"
#endif

PRIVATE void vPutc(char c)
{
    while ((u8AHI_UartReadLineStatus(UART) & E_AHI_UART_LS_THRE) == 0);
    vAHI_UartWriteData(UART, (uint8)c);
}

PRIVATE void vPuts(const char *s)
{
    while (*s)
    {
        vPutc(*s++);
    }
}

PRIVATE void vNewLine(void)
{
    vPutc(CR);
    vPutc(LF);
}

PRIVATE void vPutHex(uint32 u32Value, uint8 u8Digits)
{
    static const char acHex[] = "0123456789abcdef";

    while (u8Digits--)
    {
        vPutc(acHex[(u32Value >> (u8Digits * 4)) & 0xf]);
    }
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

PRIVATE void vDelay(uint32 u32Loops)
{
    volatile uint32 i;

    for (i = 0; i < u32Loops; i++);
}

PRIVATE void vDump(void)
{
    uint8 au8Buf[MAX_SEG_LEN];
    uint8 u8SegLen = 0;
    uint16 u16Segments = u16AHI_InitialiseEEP(&u8SegLen);
    uint32 u32Crc = 0xFFFFFFFFUL;
    uint16 s;
    uint8 i;

    if (u8SegLen > MAX_SEG_LEN)
    {
        u8SegLen = MAX_SEG_LEN;
    }

    vNewLine();
    vPuts("EEP ");
    vPutHex(u16Segments, 4);
    vPutc(' ');
    vPutHex(u8SegLen, 2);
    vNewLine();

    for (s = 0; s < u16Segments; s++)
    {
        if (iAHI_ReadDataFromEEPROMsegment(s, 0, au8Buf, u8SegLen) != 0)
        {
            vPuts("ERR ");
            vPutHex(s, 4);
            vNewLine();
            return;
        }
        u32Crc = u32Crc32(u32Crc, au8Buf, u8SegLen);

        vPutc('S');
        vPutHex(s, 4);
        vPutc(':');
        for (i = 0; i < u8SegLen; i++)
        {
            vPutHex(au8Buf[i], 2);
        }
        vNewLine();
    }

    vPuts("END ");
    vPutHex(u32Crc ^ 0xFFFFFFFFUL, 8);
    vNewLine();
}

#ifdef EEPROM_IMAGE
PRIVATE void vRestore(void)
{
    uint8 au8Buf[MAX_SEG_LEN];
    uint8 u8SegLen = 0;
    uint16 u16Segments = u16AHI_InitialiseEEP(&u8SegLen);
    uint16 s;
    uint16 u16Bad = 0;

    vNewLine();
    if ((u8SegLen != MAX_SEG_LEN) || ((uint32)u16Segments * u8SegLen != EEPROM_IMAGE_SIZE))
    {
        vPuts("RESTORE REFUSED: EEPROM geometry differs from the image");
        vNewLine();
        return;
    }

    vPuts("RESTORE START");
    vNewLine();
    for (s = 0; s < u16Segments; s++)
    {
        const uint8 *pu8Src = &au8EepromImage[(uint32)s * u8SegLen];
        uint8 i;
        bool_t bOk;

        (void)iAHI_EraseEEPROMsegment(s);
        (void)iAHI_WriteDataIntoEEPROMsegment(s, 0, (uint8 *)pu8Src, u8SegLen);
        bOk = (iAHI_ReadDataFromEEPROMsegment(s, 0, au8Buf, u8SegLen) == 0);
        for (i = 0; bOk && (i < u8SegLen); i++)
        {
            bOk = (au8Buf[i] == pu8Src[i]);
        }
        if (!bOk)
        {
            u16Bad++;
            vPuts("BAD ");
            vPutHex(s, 4);
            vNewLine();
        }
    }
    vPuts(u16Bad ? "RESTORE FAILED" : "RESTORE OK");
    vNewLine();
}

/* Collect a line from UART0; returns TRUE when the restore command arrived */
PRIVATE bool_t bPollCommand(void)
{
    static char acLine[24];
    static uint8 u8Len;

    while (u8AHI_UartReadLineStatus(UART) & E_AHI_UART_LS_DR)
    {
        char c = (char)u8AHI_UartReadData(UART);

        if ((c == CR) || (c == LF))
        {
            bool_t bMatch = (u8Len == sizeof(RESTORE_COMMAND) - 1);
            uint8 i;

            for (i = 0; bMatch && (i < u8Len); i++)
            {
                bMatch = (acLine[i] == RESTORE_COMMAND[i]);
            }
            u8Len = 0;
            if (bMatch)
            {
                return TRUE;
            }
        }
        else if (u8Len < sizeof(acLine))
        {
            acLine[u8Len++] = c;
        }
    }
    return FALSE;
}
#endif

PUBLIC void AppColdStart(void)
{
    vAHI_WatchdogStop();

    /* run from the 32 MHz crystal so the baud rate is right */
    while (bAHI_GetClkSource() == TRUE);
    bAHI_SetClockRate(3);
    (void)u32AHI_Init();

    vAHI_UartEnable(UART);
    vAHI_UartReset(UART, TRUE, TRUE);
    vAHI_UartReset(UART, FALSE, FALSE);
    vAHI_UartSetClockDivisor(UART, E_AHI_UART_RATE_115200);
    vAHI_UartSetControl(UART, E_AHI_UART_EVEN_PARITY, E_AHI_UART_PARITY_DISABLE,
                        E_AHI_UART_WORD_LEN_8, E_AHI_UART_1_STOP_BIT, E_AHI_UART_RTS_LOW);

    /* blue LED on: dumper is running */
    vAHI_DioSetDirection(0, LED_BLUE);
    vAHI_DioSetOutput(0, LED_BLUE);

    while (1)
    {
        uint32 u32Wait;

        vDump();
        for (u32Wait = 0; u32Wait < 40; u32Wait++)
        {
            vDelay(100000);
#ifdef EEPROM_IMAGE
            if (bPollCommand())
            {
                vRestore();
                break;
            }
#endif
        }
    }
}

PUBLIC void AppWarmStart(void)
{
    AppColdStart();
}
