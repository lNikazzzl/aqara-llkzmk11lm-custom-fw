#ifndef APP_DEVICE_H
#define APP_DEVICE_H

#include <jendefs.h>
#include "zcl.h"
#include "zcl_options.h"
#include "Basic.h"
#include "Identify.h"
#include "Groups.h"
#include "OnOff.h"
#include "OTA.h"
#include "hw.h"

/*
 * Endpoints laid out like the factory firmware (as interviewed by Zigbee2MQTT):
 *   EP1: Basic, Identify, Groups, OnOff, OTA client  (relay 1)
 *   EP2: Groups, OnOff                               (relay 2)
 * The other clusters the factory firmware lists are only in the simple
 * descriptor (app.zpscfg); the Lumi specific ones are handled in lumi.c.
 */
#define RELAY_EP_FIRST      1   /* relay channel N is on endpoint RELAY_EP_FIRST + N */

typedef struct
{
    tsZCL_ClusterInstance sBasicServer;
    tsZCL_ClusterInstance sIdentifyServer;
    tsZCL_ClusterInstance sGroupsServer;
    tsZCL_ClusterInstance sOnOffServer;
    tsZCL_ClusterInstance sOTAClient;
} tsEp1Clusters __attribute__ ((aligned(4)));

typedef struct
{
    tsZCL_EndPointDefinition sEndPoint;
    tsEp1Clusters sClusterInstance;

    tsCLD_Basic sBasicServerCluster;
    tsCLD_Identify sIdentifyServerCluster;
    tsCLD_IdentifyCustomDataStructure sIdentifyServerCustomDataStructure;
    tsCLD_Groups sGroupsServerCluster;
    tsCLD_GroupsCustomDataStructure sGroupsServerCustomDataStructure;
    tsCLD_OnOff sOnOffServerCluster;
    tsCLD_OnOffCustomDataStructure sOnOffServerCustomDataStructure;
    tsCLD_AS_Ota sCLD_OTA;
    tsOTA_Common sCLD_OTA_CustomDataStruct;
} tsEp1Device;

typedef struct
{
    tsZCL_ClusterInstance sGroupsServer;
    tsZCL_ClusterInstance sOnOffServer;
} tsEp2Clusters __attribute__ ((aligned(4)));

typedef struct
{
    tsZCL_EndPointDefinition sEndPoint;
    tsEp2Clusters sClusterInstance;

    tsCLD_Groups sGroupsServerCluster;
    tsCLD_GroupsCustomDataStructure sGroupsServerCustomDataStructure;
    tsCLD_OnOff sOnOffServerCluster;
    tsCLD_OnOffCustomDataStructure sOnOffServerCustomDataStructure;
} tsEp2Device;

extern tsEp1Device sEp1;
extern tsEp2Device sEp2;

PUBLIC teZCL_Status DEV_eRegisterEndpoints(tfpZCL_ZCLCallBackFunction cbCallBack);
PUBLIC void DEV_vResetStructures(void);
PUBLIC tsCLD_OnOff *DEV_psOnOff(uint8 u8Channel);

#endif /* APP_DEVICE_H */
