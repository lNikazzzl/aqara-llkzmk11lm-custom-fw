#ifndef INPUTS_H
#define INPUTS_H

#include <jendefs.h>

/*
 * How the wall switch inputs S1/S2 drive the relays:
 *   0 - momentary push button: toggle the relay on each press
 *   1 - rocker (toggle) switch: toggle the relay on every change of position
 */
#ifndef INPUT_ROCKER_MODE
#define INPUT_ROCKER_MODE   1
#endif

#define INPUT_SCAN_MS               10
#define INPUT_DEBOUNCE_MS           50
#define BTN_LONG_PRESS_MS           5000

PUBLIC void INPUTS_vInit(void);
PUBLIC bool_t INPUTS_bPairButtonHeld(void);
PUBLIC void INPUTS_cbTimerScan(void *pvParam);

#endif /* INPUTS_H */
