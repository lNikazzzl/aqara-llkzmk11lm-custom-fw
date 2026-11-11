#ifndef NVM_CLEAN_H
#define NVM_CLEAN_H

#include <jendefs.h>

/*
 * Clean hand-over of the EEPROM between this firmware and the factory one.
 *
 * Both firmwares keep the Zigbee stack records in the NXP PDM under the same
 * ids (0xF000.., 0xF100..), but they were built with different SDK builds and
 * table sizes, and the factory PDM uses more segments than ours: the factory
 * firmware crashes on stack records written by us, and ours cannot rely on
 * records written by it. So whenever the firmware changes, the stack records
 * are erased and the device is paired again. Lumi's own records (ids below
 * 0x0100: calibration, settings, counters) are left alone.
 *
 * Every PDM segment starts with the 16-bit id of the record it belongs to,
 * which is what the raw access below goes by.
 *
 * The factory firmware updates its own records on every start and this
 * firmware never writes them, so a changed fingerprint of those records
 * means the factory firmware ran since our last start.
 */

/* CRC of every segment holding a Lumi record (id below 0x0100) */
PUBLIC uint32 NVM_u32FactoryFingerprint(void);

/* Raw-erase every PDM segment of the stack (0xF000..0xF1FF) and of this
 * firmware's PDM records (0xA100..0xA1FF). Call before PDM_eInitialise() or
 * right before a reset. Returns the number of segments erased. */
PUBLIC uint8 NVM_u8EraseStackRecords(void);

#endif /* NVM_CLEAN_H */
