/*
 * PL7211 power meter probe (read only).
 *
 * Wiring (power board LM16-DCC-B-T2): SPI_CLK = DO0, SPI_DI = DIO18 (MOSI),
 * SPI_DO = DO1 (MISO), CS = DIO19, RSTN = DIO3 (10k pull-up), MODE pulled up (R5).
 * The PL7211 latches its interface from [MODE, CS] when RSTN goes high:
 * SPI needs MODE = 1 and CS = 0, so the probe resets it with CS held low.
 *
 * Protocol (Prolific DS-71160901, SPI without CRC): CS low, CMD, ADH, ADL, then
 * data bytes, CS high. CMD = 0x30 | n is a read without CRC, auto increment.
 *
 * The dumps go to the coordinator as manufacturer specific genBasic reports
 * (octet strings, attributes 0xFFF5/0xFFF6), visible in the Z2M debug log.
 * Nothing is ever written to the PL7211.
 */
#ifndef PL7211_H
#define PL7211_H

#include <jendefs.h>

PUBLIC void PL7211_vTick100ms(void);

#endif /* PL7211_H */
