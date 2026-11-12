#ifndef LUMI_H
#define LUMI_H

#include <jendefs.h>
#include "zps_apl_af.h"

/*
 * Lumi manufacturer specific protocol used by Zigbee2MQTT for LLKZMK11LM
 * (zigbee-herdsman-converters, src/lib/lumi.ts):
 *  - genBasic 0xFFF0 write: power outage memory (magic payloads)
 *  - genBinaryOutput 0xFF06 write: interlock
 *  - genBasic 0xFF01 report: "heartbeat" struct with device temperature (3),
 *    power on count (5) and relay states (100, 101)
 */
#define LUMI_MANUFACTURER_CODE      0x115F

typedef struct
{
    void (*pfSetInterlock)(bool_t bOn);
    void (*pfSetOutageMemory)(bool_t bOn);
} tsLumiCallbacks;

PUBLIC void LUMI_vInit(const tsLumiCallbacks *psCallbacks);
/* Returns TRUE if the frame was a Lumi specific command and has been handled */
PUBLIC bool_t LUMI_bHandleDataIndication(ZPS_tsAfDataIndEvent *psInd);
PUBLIC void LUMI_vSendHeartbeat(uint16 u16PowerOnCount, bool_t bRelay1, bool_t bRelay2);
PUBLIC int8 LUMI_i8ChipTemperature(void);
/* Manufacturer specific genBasic report of one octet string attribute (debug dumps) */
PUBLIC void LUMI_vSendOctetReport(uint16 u16Attr, const uint8 *pu8Data, uint8 u8Len);

#endif /* LUMI_H */
