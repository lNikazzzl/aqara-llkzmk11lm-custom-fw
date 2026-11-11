#ifndef APP_STORE_H
#define APP_STORE_H

#include <jendefs.h>
#include "hw.h"

/*
 * Settings of this firmware, kept outside the PDM.
 *
 * The factory firmware uses EEPROM segments 0..127 through the NXP PDM and
 * both firmwares share that area for the Zigbee stack records. Our own data
 * lives in segments STORE_FIRST_SEGMENT.. which the factory firmware never
 * touches, so switching firmware over the air does not corrupt its records.
 * The factory data ends at segment 127; we keep a margin above it.
 */
#define STORE_FIRST_SEGMENT     192
#define STORE_SLOTS             16      /* round robin for wear levelling */

typedef struct
{
    uint8 au8OnOff[RELAY_CHANNELS];
    uint8 au8StartUp[RELAY_CHANNELS];
    uint8 bInterlock;
    uint8 bOutageMemory;
    uint16 u16PowerOnCount;
} tsAppData;

/* Loads the newest valid slot, or defaults if there is none */
PUBLIC void STORE_vLoad(tsAppData *psData);
PUBLIC void STORE_vSave(const tsAppData *psData);

#endif /* APP_STORE_H */
