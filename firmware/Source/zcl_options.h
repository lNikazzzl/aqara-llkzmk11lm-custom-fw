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
 * ZCL configuration for the two-channel relay.
 */
#ifndef ZCL_OPTIONS_H
#define ZCL_OPTIONS_H

#include <jendefs.h>
#include "rnd_pub.h"        /* RND_u32GetRand in the OTA timing macros */
#include "app_version.h"

/* Lumi; used for manufacturer specific ZCL frames (the node descriptor keeps NXP 0x1037) */
#define ZCL_MANUFACTURER_CODE               0x115F
#define ZCL_NUMBER_OF_ENDPOINTS             2
#define ZLO_NUMBER_DEVICES                  2
#define APPLICATION_DEVICE_VERSION          1

#define ZCL_DISABLE_DEFAULT_RESPONSES       (FALSE)
#define ZCL_DISABLE_APS_ACK                 (FALSE)
#define ZCL_NUMBER_OF_APPLICATION_TIMERS    3
#define NUM_ENDPOINT_RECORDS                2
#define NUM_GROUP_RECORDS                   4

#define ZCL_ALLOW_WILD_CARD_PROFILE
#define ZCL_ATTRIBUTE_READ_SERVER_SUPPORTED
#define ZCL_ATTRIBUTE_WRITE_SERVER_SUPPORTED
#define ZCL_ATTRIBUTE_REPORTING_SERVER_SUPPORTED
#define ZCL_CONFIGURE_ATTRIBUTE_REPORTING_SERVER_SUPPORTED
#define ZCL_READ_ATTRIBUTE_REPORTING_CONFIGURATION_SERVER_SUPPORTED

/* one OnOff report slot per endpoint */
#define ZCL_NUMBER_OF_REPORTS               2
#define CLD_BIND_SERVER
#define MAX_NUM_BIND_QUEUE_BUFFERS          ZCL_NUMBER_OF_REPORTS
#define MAX_PDU_BIND_QUEUE_PAYLOAD_SIZE     (24)

/* Clusters */
#define CLD_BASIC
#define BASIC_SERVER

#define CLD_IDENTIFY
#define IDENTIFY_SERVER

#define CLD_GROUPS
#define GROUPS_SERVER

#define CLD_ONOFF
#define ONOFF_SERVER

/* Basic */
#define CLD_BAS_ATTR_APPLICATION_VERSION
#define CLD_BAS_ATTR_STACK_VERSION
#define CLD_BAS_ATTR_HARDWARE_VERSION
#define CLD_BAS_ATTR_MANUFACTURER_NAME
#define CLD_BAS_ATTR_MODEL_IDENTIFIER
#define CLD_BAS_ATTR_DATE_CODE
#define CLD_BAS_ATTR_SW_BUILD_ID

/* identity of the factory firmware, so Zigbee2MQTT uses its LLKZMK11LM definition */
#define BAS_MANUF_NAME_STRING               "LUMI"
#define BAS_MODEL_ID_STRING                 "lumi.relay.c2acn01"
#define BAS_DATE_STRING                     APP_BUILD_DATE
/* the only visible difference to the factory firmware */
#define BAS_SW_BUILD_STRING                 "custom-" APP_VERSION_STRING

#define CLD_BAS_APP_VERSION                 (1)
#define CLD_BAS_STACK_VERSION               (2)
#define CLD_BAS_HARDWARE_VERSION            (19)
#define CLD_BAS_MANUF_NAME_SIZE             (sizeof(BAS_MANUF_NAME_STRING) - 1)
#define CLD_BAS_MODEL_ID_SIZE               (sizeof(BAS_MODEL_ID_STRING) - 1)
#define CLD_BAS_DATE_SIZE                   (sizeof(BAS_DATE_STRING) - 1)
#define CLD_BAS_SW_BUILD_SIZE               (sizeof(BAS_SW_BUILD_STRING) - 1)
#define CLD_BAS_POWER_SOURCE                E_CLD_BAS_PS_SINGLE_PHASE_MAINS
#define CLD_BAS_DEVICE_CLASS                (0)
#define CLD_BAS_CMD_RESET_TO_FACTORY_DEFAULTS

/* Identify */
#define CLD_IDENTIFY_TICKS_PER_SECOND       10
#define CLD_IDENTIFY_10HZ_TICK

/* Groups */
#define CLD_GROUPS_MAX_NUMBER_OF_GROUPS     8
#define CLD_GROUPS_DISABLE_NAME_SUPPORT

/* OnOff */
#define CLD_ONOFF_ATTR_ON_TIME
#define CLD_ONOFF_ATTR_OFF_WAIT_TIME
#define CLD_ONOFF_ATTR_STARTUP_ONOFF
#define CLD_ONOFF_CMD_ON_WITH_TIMED_OFF

/* restore the previous state after a power cut */
#define CLD_ONOFF_START_MODE                E_CLD_ONOFF_STARTUPONOFF_PREVIOUS_VALUE

/* OTA client, settings from NXP JN-AN-1218 (production timings) */
#define CLD_OTA
#define OTA_CLIENT
#define OTA_ACKS_ON                         FALSE
#define OTA_MAX_CO_PROCESSOR_IMAGES         0
#define OTA_CLD_ATTR_CURRENT_FILE_VERSION
#define OTA_MAX_BLOCK_SIZE                  48      /* multiple of 16 for internal flash */
#define OTA_TIME_INTERVAL_BETWEEN_REQUESTS  RND_u32GetRand(10, 20)
#define OTA_TIME_INTERVAL_BETWEEN_RETRIES   RND_u32GetRand(10, 20)
#define CLD_OTA_MAX_BLOCK_PAGE_REQ_RETRIES  240
#define OTA_STRING_COMPARE
#define OTA_MAX_IMAGES_PER_ENDPOINT         1

#endif /* ZCL_OPTIONS_H */
