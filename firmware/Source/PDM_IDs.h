#ifndef PDM_IDS_H
#define PDM_IDS_H

/*
 * The PDM (EEPROM segments 0..126) is shared with the factory firmware, which
 * was built from NXP JN-AN-1218 and uses the same 0xA1xx application ids.
 * This firmware keeps its own settings outside the PDM (app_store.h) and only
 * uses the records whose content is the same in both firmwares.
 */
/* 0..126: segment 127 holds non-PDM factory data (first byte 0xE0), never touch it */
#define PDM_SEGMENTS                127

/* OTA client progress, tsOTA_PersistedData from the SDK (JN-AN-1218 id) */
#define PDM_ID_OTA_DATA             (0xA102)

#endif /* PDM_IDS_H */
