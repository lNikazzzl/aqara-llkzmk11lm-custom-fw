#ifndef RELAY_H
#define RELAY_H

#include <jendefs.h>

/* Coil pulse length for the ADW1203HL latching relays */
#define RELAY_PULSE_MS      50

PUBLIC void RELAY_vInit(void);
/* Queue a set/reset pulse for channel 0/1; pulses are issued one at a time */
PUBLIC void RELAY_vSet(uint8 u8Channel, bool_t bOn);
PUBLIC void RELAY_cbTimer(void *pvParam);

#endif /* RELAY_H */
