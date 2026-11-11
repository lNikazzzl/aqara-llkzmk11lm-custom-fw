#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <jendefs.h>
#include "ZQueue.h"
#include "tsv_pub.h"
#include "bdb_api.h"

typedef enum
{
    APP_E_EVENT_NONE = 0,
    APP_E_EVENT_BUTTON_SHORT,   /* on-board button released before the long press time */
    APP_E_EVENT_BUTTON_LONG,    /* on-board button held for BTN_LONG_PRESS_MS */
    APP_E_EVENT_SWITCH,         /* wall switch input S1/S2 operated, u8Channel = 0/1 */
} APP_teEventType;

typedef struct
{
    APP_teEventType eType;
    uint8 u8Channel;
} APP_tsEvent;

extern PUBLIC uint8 u8TimerInputs;
extern PUBLIC uint8 u8TimerRelay;
extern PUBLIC uint8 u8TimerLed;
extern PUBLIC uint8 u8TimerZclTick;
extern PUBLIC uint8 u8TimerSteering;
extern PUBLIC uint8 u8TimerRadioRecal;

extern PUBLIC tszQueue APP_msgBdbEvents;
extern PUBLIC tszQueue APP_msgAppEvents;

extern PUBLIC tszQueue zps_msgMlmeDcfmInd;
extern PUBLIC tszQueue zps_msgMcpsDcfmInd;
extern PUBLIC tszQueue zps_msgMcpsDcfm;
extern PUBLIC tszQueue zps_TimeEvents;

#endif /* APP_MAIN_H */
