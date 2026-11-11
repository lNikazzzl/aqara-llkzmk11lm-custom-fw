/*
 * Portions of this file are derived from NXP software (JN-AN-1218 and the
 * JN-SW-4170 Zigbee 3.0 SDK, Copyright NXP B.V. 2017). The NXP notice follows.
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5168, JN5179].
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright NXP B.V. 2017. All rights reserved
 */

/*
 * ZCL endpoint registration, modelled on eZLO_RegisterOnOffLightEndPoint()
 * from the SDK but with the cluster set of the factory firmware.
 */
#include <jendefs.h>
#include <string.h>
#include "app_device.h"

tsEp1Device sEp1;
tsEp2Device sEp2;

PRIVATE void vInitEndpoint(tsZCL_EndPointDefinition *psEndPoint, uint8 u8Endpoint, uint16 u16Clusters,
                           tsZCL_ClusterInstance *psInstances, tfpZCL_ZCLCallBackFunction cbCallBack)
{
    psEndPoint->u8EndPointNumber = u8Endpoint;
    psEndPoint->u16ManufacturerCode = ZCL_MANUFACTURER_CODE;
    psEndPoint->u16ProfileEnum = HA_PROFILE_ID;
    psEndPoint->bIsManufacturerSpecificProfile = FALSE;
    psEndPoint->u16NumberOfClusters = u16Clusters;
    psEndPoint->psClusterInstance = psInstances;
    psEndPoint->bDisableDefaultResponse = ZCL_DISABLE_DEFAULT_RESPONSES;
    psEndPoint->pCallBackFunctions = cbCallBack;
}

PRIVATE teZCL_Status eRegisterEp1(tfpZCL_ZCLCallBackFunction cbCallBack)
{
    tsEp1Device *d = &sEp1;

    vInitEndpoint(&d->sEndPoint, RELAY_EP_FIRST,
                  sizeof(tsEp1Clusters) / sizeof(tsZCL_ClusterInstance),
                  (tsZCL_ClusterInstance *)&d->sClusterInstance, cbCallBack);

    if ((eCLD_BasicCreateBasic(&d->sClusterInstance.sBasicServer, TRUE, &sCLD_Basic,
                               &d->sBasicServerCluster, &au8BasicClusterAttributeControlBits[0]) != E_ZCL_SUCCESS) ||
        (eCLD_IdentifyCreateIdentify(&d->sClusterInstance.sIdentifyServer, TRUE, &sCLD_Identify,
                                     &d->sIdentifyServerCluster, &au8IdentifyAttributeControlBits[0],
                                     &d->sIdentifyServerCustomDataStructure) != E_ZCL_SUCCESS) ||
        (eCLD_GroupsCreateGroups(&d->sClusterInstance.sGroupsServer, TRUE, &sCLD_Groups,
                                 &d->sGroupsServerCluster, &au8GroupsAttributeControlBits[0],
                                 &d->sGroupsServerCustomDataStructure, &d->sEndPoint) != E_ZCL_SUCCESS) ||
        (eCLD_OnOffCreateOnOff(&d->sClusterInstance.sOnOffServer, TRUE, &sCLD_OnOff,
                               &d->sOnOffServerCluster, &au8OnOffAttributeControlBits[0],
                               &d->sOnOffServerCustomDataStructure) != E_ZCL_SUCCESS) ||
        (eOTA_Create(&d->sClusterInstance.sOTAClient, FALSE, &sCLD_OTA, &d->sCLD_OTA,
                     RELAY_EP_FIRST, NULL, &d->sCLD_OTA_CustomDataStruct) != E_ZCL_SUCCESS))
    {
        return E_ZCL_FAIL;
    }
    return eZCL_Register(&d->sEndPoint);
}

PRIVATE teZCL_Status eRegisterEp2(tfpZCL_ZCLCallBackFunction cbCallBack)
{
    tsEp2Device *d = &sEp2;

    vInitEndpoint(&d->sEndPoint, RELAY_EP_FIRST + 1,
                  sizeof(tsEp2Clusters) / sizeof(tsZCL_ClusterInstance),
                  (tsZCL_ClusterInstance *)&d->sClusterInstance, cbCallBack);

    if ((eCLD_GroupsCreateGroups(&d->sClusterInstance.sGroupsServer, TRUE, &sCLD_Groups,
                                 &d->sGroupsServerCluster, &au8GroupsAttributeControlBits[0],
                                 &d->sGroupsServerCustomDataStructure, &d->sEndPoint) != E_ZCL_SUCCESS) ||
        (eCLD_OnOffCreateOnOff(&d->sClusterInstance.sOnOffServer, TRUE, &sCLD_OnOff,
                               &d->sOnOffServerCluster, &au8OnOffAttributeControlBits[0],
                               &d->sOnOffServerCustomDataStructure) != E_ZCL_SUCCESS))
    {
        return E_ZCL_FAIL;
    }
    return eZCL_Register(&d->sEndPoint);
}

PUBLIC teZCL_Status DEV_eRegisterEndpoints(tfpZCL_ZCLCallBackFunction cbCallBack)
{
    teZCL_Status eStatus = eRegisterEp1(cbCallBack);

    if (eStatus != E_ZCL_SUCCESS)
    {
        return eStatus;
    }
    return eRegisterEp2(cbCallBack);
}

PUBLIC void DEV_vResetStructures(void)
{
    memset(&sEp1, 0, sizeof(sEp1));
    memset(&sEp2, 0, sizeof(sEp2));
}

PUBLIC tsCLD_OnOff *DEV_psOnOff(uint8 u8Channel)
{
    return (u8Channel == 0) ? &sEp1.sOnOffServerCluster : &sEp2.sOnOffServerCluster;
}
