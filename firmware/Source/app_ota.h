#ifndef APP_OTA_H
#define APP_OTA_H

#include <jendefs.h>
#include "OTA.h"
#include "zps_apl_af.h"

/*
 * OTA client compatible with the factory firmware: Lumi manufacturer code,
 * the same image type and header string, internal flash storage. The image
 * this firmware is shipped in carries OTA file version APP_OTA_FILE_VERSION
 * (above the factory 47) while the embedded header reports
 * APP_OTA_REPORTED_VERSION, so Zigbee2MQTT keeps offering the factory image.
 */
#define APP_OTA_MANUFACTURER        0x115F
#define APP_OTA_IMAGE_TYPE          0x0101

PUBLIC void APP_OTA_vInit(void);
PUBLIC void APP_OTA_vTick1s(void);
PUBLIC void APP_OTA_vHandleEvent(tsOTA_CallBackMessage *psCallBackMessage);
PUBLIC void APP_OTA_vResetPersist(void);

#endif /* APP_OTA_H */
