/*
 * Board pin map: Aqara LLKZMK11LM (MCU module LM16-DCC-A-T1, power board LM16-DCC-B-T2).
 * Source: re-engr.ru schematics of both boards.
 */
#ifndef HW_H
#define HW_H

#include <jendefs.h>

#define DIO(n)              (1UL << (n))

/* LEDs on the MCU module, anode to +3V3 -> active low */
#define LED_BLUE            DIO(1)
#define LED_RED             DIO(2)

/* On-board button S1, 100k pull-up -> active low */
#define BTN_PAIR            DIO(17)

/* Wall switch inputs S1/S2 (dry contact to COM), 10k pull-up -> active low */
#define IN_S1               DIO(15)
#define IN_S2               DIO(14)

/* Bistable relays K1/K2, two coils each, driven by 2N7002 -> active high pulse */
#define RELAY1_ON           DIO(10)
#define RELAY1_OFF          DIO(8)
#define RELAY2_ON           DIO(11)
#define RELAY2_OFF          DIO(9)
#define RELAY_ALL           (RELAY1_ON | RELAY1_OFF | RELAY2_ON | RELAY2_OFF)

/* PL7211 power meter (not used yet): SPI on DO0/DIO18/DIO19/DO1, reset DIO3 */

#define RELAY_CHANNELS      2

#endif /* HW_H */
