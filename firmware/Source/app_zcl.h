#ifndef APP_ZCL_H
#define APP_ZCL_H

#include <jendefs.h>
#include "zps_apl_af.h"
#include "app_device.h"

PUBLIC void APP_ZCL_vInitialise(void);
PUBLIC void APP_ZCL_vEventHandler(ZPS_tsAfEvent *psStackEvent);
PUBLIC void APP_ZCL_cbTimerTick(void *pvParam);
PUBLIC bool_t APP_ZCL_bIsRelayEndpoint(uint8 u8Endpoint);
PUBLIC void APP_ZCL_vToggleRelay(uint8 u8Channel);
/* Reports relay states and sends the Lumi heartbeat, e.g. after joining */
PUBLIC void APP_ZCL_vReportAll(void);

#endif /* APP_ZCL_H */
