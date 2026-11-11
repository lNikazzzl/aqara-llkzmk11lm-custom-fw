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
 * OTA client. Based on app_ota_client.c from NXP JN-AN-1218 (the same code
 * base the factory firmware was built from), with server discovery replaced
 * by the coordinator: Zigbee2MQTT serves OTA images from 0x0000 endpoint 1.
 */
#include <jendefs.h>
#include <string.h>
#include "dbg.h"
#include "PDM.h"
#include "pdum_gen.h"
#include "zps_apl_af.h"
#include "zps_apl_aib.h"
#include "rnd_pub.h"
#include "OTA.h"
#include "zcl_options.h"

#include "PDM_IDs.h"
#include "app_device.h"
#include "app_node.h"
#include "app_ota.h"
#include "led.h"

#ifdef DEBUG_OTA
#define TRACE_OTA TRUE
#else
#define TRACE_OTA FALSE
#endif

#define OTA_SERVER_ADDR                 0x0000
#define OTA_SERVER_EP                   1
#define OTA_FIRST_QUERY_MIN_SEC         60
#define OTA_FIRST_QUERY_MAX_SEC         180
#define OTA_QUERY_PERIOD_SEC            (60 * 60)
#define OTA_DL_STALL_TIMEOUT_SEC        (15 * 60)
/* the factory client checks the link key once this much of the image is in flash */
#define OTA_LINKKEY_CHECK_OFFSET        1100

/* internal storage: the new image goes into the upper half of the 512K flash */
#define OTA_START_SECTOR                8
#define OTA_SECTOR_SIZE                 (32 * 1024)
#define OTA_MAX_SECTORS                 8

/*
 * Serialisation data at fixed places of the image (see AppBuildZBProOta.ld).
 * The link key must be the one the factory firmware has, it compares it in
 * every image it downloads.
 */
PUBLIC uint8 au8MacAddress[8] __attribute__ ((section (".ro_mac_address"))) = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
PUBLIC uint8 s_au8LnkKeyArray[16] __attribute__ ((section (".ro_se_lnkKey"))) = {
    'Z', 'i', 'g', 'B', 'e', 'e', 'A', 'l', 'l', 'i', 'a', 'n', 'c', 'e', '0', '9'
};

/* _FlsLinkKey and _flash_start come from OTA.h (linker symbols) */
extern tsNvmDefs sNvmDefsStruct;

PRIVATE tsOTA_PersistedData sOTA_PersistedData;
PRIVATE uint32 u32SecondsToQuery;
PRIVATE uint32 u32DownloadIdleSec;
PRIVATE bool_t bDownloading;
PRIVATE bool_t bWaitUpgradeTime;
PRIVATE bool_t bKeyVerified;

PRIVATE void vPersist(void)
{
    sOTA_PersistedData = sEp1.sCLD_OTA_CustomDataStruct.sOTACallBackMessage.sPersistedData;
    PDM_eSaveRecordData(PDM_ID_OTA_DATA, &sOTA_PersistedData, sizeof(sOTA_PersistedData));
}

PRIVATE void vSetServer(void)
{
    eOTA_SetServerAddress(RELAY_EP_FIRST, ZPS_eAplAibGetApsTrustCenterAddress(), OTA_SERVER_ADDR);
}

PRIVATE void vQueryNextImage(void)
{
    tsOTA_ImageHeader sHeader;
    tsOTA_QueryImageRequest sRequest;
    tsZCL_Address sAddress;
    teZCL_Status eStatus;

    eOTA_GetCurrentOtaHeader(RELAY_EP_FIRST, FALSE, &sHeader);
    vSetServer();

    sAddress.eAddressMode = E_ZCL_AM_SHORT;
    sAddress.uAddress.u16DestinationAddress = OTA_SERVER_ADDR;
    sRequest.u32CurrentFileVersion = sHeader.u32FileVersion;
    sRequest.u16HardwareVersion = 0;
    sRequest.u16ImageType = sHeader.u16ImageType;
    sRequest.u16ManufacturerCode = sHeader.u16ManufacturerCode;
    sRequest.u8FieldControl = 0;

    eStatus = eOTA_ClientQueryNextImageRequest(RELAY_EP_FIRST, OTA_SERVER_EP, &sAddress, &sRequest);
    DBG_vPrintf(TRACE_OTA, "OTA: query next image, version 0x%08x, status %d\n", sHeader.u32FileVersion, eStatus);
}

/* Compare the link key in the downloaded image with ours, like the factory client does */
PRIVATE uint8 u8VerifyLinkKey(tsOTA_CallBackMessage *psMsg)
{
    uint8 au8Downloaded[16];
    uint32 u32Location;
    uint8 i;

    if (bKeyVerified ||
        (psMsg->uMessage.sImageBlockResponsePayload.uMessage.sBlockPayloadSuccess.u32FileOffset <= OTA_LINKKEY_CHECK_OFFSET))
    {
        return OTA_STATUS_SUCCESS;
    }
    bKeyVerified = TRUE;

    u32Location = (uint32)&_FlsLinkKey - (uint32)&_flash_start;
    u32Location += psMsg->u8ImageStartSector[0] * sNvmDefsStruct.u32SectorSize;
    bAHI_FullFlashRead(u32Location, sizeof(au8Downloaded), au8Downloaded);

    for (i = 0; i < sizeof(au8Downloaded); i++)
    {
        if (au8Downloaded[i] != s_au8LnkKeyArray[i])
        {
            DBG_vPrintf(TRUE, "OTA: link key mismatch in the new image, abort\n");
            bKeyVerified = FALSE;
            return OTA_STATUS_ABORT;
        }
    }
    return OTA_STATUS_SUCCESS;
}

PRIVATE void vAbortDownload(tsOTA_CallBackMessage *psMsg)
{
    tsOTA_UpgradeEndRequestPayload sPayload;
    tsZCL_Address sAddress;

    sPayload.u32FileVersion = psMsg->uMessage.sImageBlockResponsePayload.uMessage.sBlockPayloadSuccess.u32FileVersion;
    sPayload.u16ImageType = psMsg->uMessage.sImageBlockResponsePayload.uMessage.sBlockPayloadSuccess.u16ImageType;
    sPayload.u16ManufacturerCode = psMsg->uMessage.sImageBlockResponsePayload.uMessage.sBlockPayloadSuccess.u16ManufacturerCode;
    sPayload.u8Status = OTA_STATUS_ABORT;

    sAddress.eAddressMode = E_ZCL_AM_SHORT_NO_ACK;
    sAddress.uAddress.u16DestinationAddress = psMsg->sPersistedData.u16ServerShortAddress;
    eOTA_ClientUpgradeEndRequest(RELAY_EP_FIRST, psMsg->sPersistedData.u8DstEndpoint, &sAddress, &sPayload);
}

PRIVATE void vLoadPersistedData(void)
{
    uint16 u16BytesRead;

    if (PDM_eReadDataFromRecord(PDM_ID_OTA_DATA, &sOTA_PersistedData, sizeof(sOTA_PersistedData),
                                &u16BytesRead) != PDM_E_STATUS_OK)
    {
        memset(&sOTA_PersistedData, 0, sizeof(sOTA_PersistedData));
    }
    if (sOTA_PersistedData.u32RequestBlockRequestTime != 0)
    {
        sOTA_PersistedData.u32RequestBlockRequestTime = 10;
    }
    sOTA_PersistedData.u8Retry = 0;
}

PUBLIC void APP_OTA_vInit(void)
{
    tsNvmDefs sNvmDefs;
    teZCL_Status eStatus;
    uint8 au8CAPublicKey[22];
    uint8 au8StartSector[1] = { OTA_START_SECTOR };
    tsOTA_ImageHeader sHeader;
    volatile uint8 u8MacByte;

    /*
     * Reference the MAC placeholder so --gc-sections keeps .ro_mac_address:
     * without it the OTA header and the link key move 16 bytes down and JET
     * writes the serialisation data over our own code (see tools/ota/make_ota.sh).
     */
    u8MacByte = au8MacAddress[0];
    (void)u8MacByte;

    vLoadPersistedData();
    eOTA_UpdateClientAttributes(RELAY_EP_FIRST, 0);

    /* after an upgrade the image may run remapped; restore the identity map */
    if (u32REG_SysRead(REG_SYS_FLASH_REMAP) & 0xf)
    {
        vREG_SysWrite(REG_SYS_FLASH_REMAP, 0xfedcba98);
        vREG_SysWrite(REG_SYS_FLASH_REMAP2, 0x76543210);
    }

    sNvmDefs.u32SectorSize = OTA_SECTOR_SIZE;
    sNvmDefs.u8FlashDeviceType = E_FL_CHIP_INTERNAL;
    vOTA_FlashInit(NULL, &sNvmDefs);

    eStatus = eOTA_AllocateEndpointOTASpace(RELAY_EP_FIRST, au8StartSector, OTA_MAX_IMAGES_PER_ENDPOINT,
                                            OTA_MAX_SECTORS, FALSE, au8CAPublicKey);
    if (eStatus != E_ZCL_SUCCESS)
    {
        DBG_vPrintf(TRUE, "OTA: allocate space failed %d\n", eStatus);
        return;
    }

    eOTA_GetCurrentOtaHeader(RELAY_EP_FIRST, FALSE, &sHeader);
    DBG_vPrintf(TRUE, "OTA: running manufacturer 0x%04x type 0x%04x version 0x%08x\n",
                sHeader.u16ManufacturerCode, sHeader.u16ImageType, sHeader.u32FileVersion);

    eOTA_RestoreClientData(RELAY_EP_FIRST, &sOTA_PersistedData, TRUE);

    u32SecondsToQuery = RND_u32GetRand(OTA_FIRST_QUERY_MIN_SEC, OTA_FIRST_QUERY_MAX_SEC);
    bDownloading = FALSE;
    bWaitUpgradeTime = FALSE;
}

PUBLIC void APP_OTA_vTick1s(void)
{
    if (!APP_bIsRunning())
    {
        return;
    }

    if (bDownloading)
    {
        if ((++u32DownloadIdleSec > OTA_DL_STALL_TIMEOUT_SEC) && !bWaitUpgradeTime)
        {
            DBG_vPrintf(TRUE, "OTA: download stalled\n");
            bDownloading = FALSE;
            u32SecondsToQuery = OTA_QUERY_PERIOD_SEC;
        }
        return;
    }

    if (u32SecondsToQuery && (--u32SecondsToQuery == 0))
    {
        vQueryNextImage();
        u32SecondsToQuery = OTA_QUERY_PERIOD_SEC;
    }
}

PUBLIC void APP_OTA_vHandleEvent(tsOTA_CallBackMessage *psMsg)
{
    u32DownloadIdleSec = 0;

    switch (psMsg->eEventId)
    {
    case E_CLD_OTA_COMMAND_QUERY_NEXT_IMAGE_RESPONSE:
        DBG_vPrintf(TRACE_OTA, "OTA: query response status %02x, version 0x%08x\n",
                    psMsg->uMessage.sQueryImageResponsePayload.u8Status,
                    psMsg->uMessage.sQueryImageResponsePayload.u32FileVersion);
        break;

    case E_CLD_OTA_INTERNAL_COMMAND_VERIFY_IMAGE_VERSION:
    {
        uint32 u32Notified = psMsg->uMessage.sImageVersionVerify.u32NotifiedImageVersion;

        /* refuse to reinstall the image we are running from */
        if ((u32Notified == psMsg->uMessage.sImageVersionVerify.u32CurrentImageVersion) ||
            (u32Notified == APP_OTA_FILE_VERSION))
        {
            DBG_vPrintf(TRUE, "OTA: image 0x%08x is the running one, ignored\n", u32Notified);
            psMsg->uMessage.sImageVersionVerify.eImageVersionVerifyStatus = E_ZCL_FAIL;
        }
        else
        {
            DBG_vPrintf(TRUE, "OTA: downloading image 0x%08x\n", u32Notified);
            bDownloading = TRUE;
            bKeyVerified = FALSE;
            LED_vSetMode(LED_E_IDENTIFY);
        }
        break;
    }

    case E_CLD_OTA_INTERNAL_COMMAND_VERIFY_STRING:
        if (psMsg->uMessage.sImageIdentification.eStatus == E_ZCL_FAIL)
        {
            DBG_vPrintf(TRUE, "OTA: header string mismatch\n");
            bDownloading = FALSE;
        }
        break;

    case E_CLD_OTA_COMMAND_BLOCK_RESPONSE:
        if (psMsg->uMessage.sImageBlockResponsePayload.u8Status == OTA_STATUS_SUCCESS)
        {
            psMsg->uMessage.sImageBlockResponsePayload.u8Status = u8VerifyLinkKey(psMsg);
            if (psMsg->uMessage.sImageBlockResponsePayload.u8Status == OTA_STATUS_ABORT)
            {
                /* the ZCL does not send the abort itself */
                vAbortDownload(psMsg);
                bDownloading = FALSE;
                LED_vSetMode(LED_E_ERROR);
            }
        }
        break;

    case E_CLD_OTA_COMMAND_UPGRADE_END_RESPONSE:
    {
        tsOTA_UpgradeEndResponsePayload *psRsp = &psMsg->uMessage.sUpgradeResponsePayload;

        DBG_vPrintf(TRUE, "OTA: download complete, switching to version 0x%08x\n", psRsp->u32FileVersion);
        if (psRsp->u32UpgradeTime != 0xFFFFFFFF)
        {
            /* upgrade in a few seconds unless the server asks for a later time */
            psRsp->u32UpgradeTime = (psRsp->u32UpgradeTime > psRsp->u32CurrentTime) ?
                                    psRsp->u32UpgradeTime - psRsp->u32CurrentTime : 5;
            psRsp->u32CurrentTime = 0;
        }
        bWaitUpgradeTime = TRUE;
        break;
    }

    case E_CLD_OTA_INTERNAL_COMMAND_SAVE_CONTEXT:
        vPersist();
        break;

    case E_CLD_OTA_INTERNAL_COMMAND_OTA_DL_ABORTED:
    case E_CLD_OTA_INTERNAL_COMMAND_FAILED_VALIDATING_UPGRADE_IMAGE:
        DBG_vPrintf(TRUE, "OTA: download aborted (event %d)\n", psMsg->eEventId);
        bDownloading = FALSE;
        bWaitUpgradeTime = FALSE;
        if (LED_eGetMode() == LED_E_IDENTIFY)
        {
            LED_vSetMode(LED_E_ERROR);
        }
        u32SecondsToQuery = OTA_QUERY_PERIOD_SEC;
        break;

    default:
        break;
    }
}

PUBLIC void APP_OTA_vResetPersist(void)
{
    memset(&sOTA_PersistedData, 0, sizeof(sOTA_PersistedData));
    PDM_eSaveRecordData(PDM_ID_OTA_DATA, &sOTA_PersistedData, sizeof(sOTA_PersistedData));
}
