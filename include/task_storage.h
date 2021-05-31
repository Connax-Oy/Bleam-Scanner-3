/**
 * @addtogroup task_storage
 * @{
 */
#ifndef BLESC_STORAGE_H__
#define BLESC_STORAGE_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"
#include "ble_gap.h"

#include "task_signature.h"

/** Time between blacklist purges.
 * @ingroup ios_solution
 */
#define MACLIST_TIMEOUT __TIMER_TICKS(APP_CONFIG_MACLIST_TIMEOUT) /**< Time for Bleam RSSI scan process. */

#define RSSI_LOWER_LIMIT_DEFAULT INT8_MIN /**< Default lower RSSI limit for scanned advertising report to be processed. */

/**@brief Version data structure. */
typedef struct  __attribute((packed)) {
    uint16_t protocol_id; /**< Protocol number; actually an 8-bit number. */
    uint16_t fw_id;       /**< Firmware version number. */
} version_t;

/**@brief Bleam Scanner node retained parameters structure. */
typedef struct  __attribute((packed)) {
    int8_t     rssi_lower_limit; /**< The lower limit of RSSI level values for accepted scan packets from Bleam */
    uint8_t    placeholder[3];   /**< A placeholder to pad this structure */
} blesc_params_t;

/**@brief Configuration data structure. */
typedef struct  __attribute((packed)) {
    uint32_t       node_id; /**< Bleam Scanner node ID */
    blesc_keys_t   keys;    /**< Bleam/Bleam Scanner communtication keys */
} configuration_t;

/**
 * Detected devices' RSSI data storage struct
 */
typedef struct {
    uint8_t  active;                                 /**< Flag that denotes if this structure is empty or not */
    uint8_t  bleam_uuid[APP_CONFIG_BLEAM_UUID_SIZE]; /**< Bleam UUID for which the RSSI data is collected */
    uint8_t  mac[BLE_GAP_ADDR_LEN];                  /**< Bleam MAC address for which the RSSI data is collected */
    uint8_t  raw[16];                                /**< iOS-identifying raw advertising data */
    uint8_t  scans_stored_cnt;                       /**< Index of last scan result stored in RSSI storage */
    int8_t   rssi[APP_CONFIG_RSSI_PER_MSG];          /**< Received Signal Strength of Bleam */
    uint8_t  aoa[APP_CONFIG_RSSI_PER_MSG];           /**< Angle of arrival of Bleam signal */
    uint32_t timestamp;                              /**< Timestamp of last received RSSI */
} blesc_model_rssi_data_t;

/** iOS RSSI data struct
 * @ingroup ios_solution
 */
typedef struct {
    uint8_t  active;                                 /**< Flag that denotes if this structure is empty or not */
    uint8_t  bleam_uuid[APP_CONFIG_BLEAM_UUID_SIZE]; /**< Bleam UUID for which the RSSI data is collected */
    uint8_t  mac[BLE_GAP_ADDR_LEN];                  /**< Bleam MAC address for which the RSSI data is collected */
    uint8_t  raw[16];                                /**< Raw iOS overflow data from raw background advertising */
    int8_t   rssi;                                   /**< Received Signal Strength of Bleam */
    uint8_t  aoa;                                    /**< Angle of arrival of Bleam signal */
    uint32_t timestamp;                              /**< Timestamp of last received RSSI */
} bleam_ios_rssi_data_t;

/** iOS MAC whitelist data struct
 * @ingroup ios_solution
 */
typedef struct {
    uint8_t  active;                                 /**< Flag that denotes if this structure is empty or not */
    uint8_t  bleam_uuid[APP_CONFIG_BLEAM_UUID_SIZE]; /**< Bleam UUID for which the RSSI data is collected */
    uint8_t  raw[16];                                /**< Raw iOS overflow data from raw background advertising */
    uint32_t timestamp;                              /**< Timestamp of last received RSSI */
} bleam_ios_raw_whitelist_t;

/** iOS MAC blacklist data struct
 * @ingroup ios_solution
 */
typedef struct {
    uint8_t  active;                /**< Flag that denotes if this structure is empty or not */
    uint8_t  raw[16];               /**< Raw iOS overflow data from raw background advertising */
    uint32_t timestamp;             /**< Timestamp of last received RSSI */
} bleam_ios_raw_blacklist_t;

/** Clear all RSSI data for a Bleam device
 *
 * @param[in] data    Pointer to RSSI scan data entry to be cleared.
 *
 * @returns Nothing.
*/
void clear_rssi_data(blesc_model_rssi_data_t *data);

/**@brief Function for providing external modules with data on a Bleam device.
 *
 * @param[in] index  Index of Bleam device data in the storage array.
 *
 * @returns Pointer to a data structure with Bleam device data.
 */
blesc_model_rssi_data_t * get_rssi_data(uint8_t index);

/**@brief Function for counting ticks between provided timestamp and current time.
 *
 * @param[in] past_timestamp    Timestamp taken in the past.
 *
 * @returns Difference between current ticks and timestamp.
 */
uint32_t how_long_ago(uint32_t past_timestamp);

/**@brief Function for adding RSSI scan data to storage
 *
 * @param[in] uuid_storage_index    Bleam device index in data storage.
 * @param[in] rssi                  Pointer to RSSI level for scanned BLEAM
 * @param[in] aoa                   Pointer to AOA data for scanned BLEAM
 *
 * @retval true if RSSI data is ready to send to Bleam now
 * @retval false otherwise
 */
bool app_blesc_save_rssi_to_storage(const uint8_t uuid_storage_index, const uint8_t *rssi, const uint8_t *aoa);

/**@brief Function for adding a new Bleam device to storage
 *
 * @param[in] p_uuid    UUID of Bleam device to add.
 * @param[in] p_mac     MAC address of Bleam device to add.
 * @param[in] p_raw     Raw advertising data of iOS Bleam device to add.
 *
 * @returns Index of Bleam device in storage.
 */
uint8_t app_blesc_save_bleam_to_storage(const uint8_t * p_uuid, const uint8_t * p_mac, const uint8_t * p_raw);

/**@brief Function for searching for a MAC address in iOS whitelist
 *
 * @param[in] p_raw    Pointer to MAC address to look for.
 *
 * @returns In case the address is present in whitelist,
 *          returns the pointer to corresponding UUID, otherwise returns a NULL pointer.
 */
uint8_t * raw_in_whitelist(const uint8_t * p_raw);

/**@brief Function for adding a MAC address to iOS whitelist
 *
 * @param[in] p_raw     Pointer to MAC address of Bleam device to add.
 * @param[in] p_uuid    Pointer to UUID of Bleam device to add.
 *
 * @retval NRF_SUCCESS            if the value is successfully added.
 * @retval NRF_ERROR_NO_MEM       if the whitelist is full.
 * @retval NRF_ERROR_INVALID_DATA if the value to add is invalid.
 */
ret_code_t add_raw_in_whitelist(const uint8_t * p_raw, uint8_t * p_uuid);

/**@brief Function for searching for a MAC address in iOS blacklist
 *
 * @param[in] p_raw    Pointer to MAC address to look for.
 *
 * @retval true in case the address is present in blacklist,
 * @retval false otherwise.
 */
bool raw_in_blacklist(const uint8_t * p_raw);

/**@brief Function for adding a MAC address to iOS blacklist
 *
 * @param[in] p_raw     Pointer to MAC address of Bleam device to add.
 *
 * @retval NRF_SUCCESS            if the value is successfully added.
 * @retval NRF_ERROR_NO_MEM       if the blacklist is full.
 * @retval NRF_ERROR_INVALID_DATA if the value to add is invalid.
 */
ret_code_t add_raw_in_blacklist(const uint8_t * p_raw);

/**@brief Function for handlind timeout event for drop_blacklist_timer_id.
 * @details Function erases all MAC addresses from iOS blacklist on timeout.
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 */
void drop_blacklist(void * p_context);

#endif // BLESC_STORAGE_H__

/** @}*/