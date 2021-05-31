/** @file task_storage.c
 *
 * @defgroup task_storage Task Storage
 * @{
 * @ingroup bleam_storage
 * @ingroup blesc_tasks
 *
 * @brief Data storage and manipulation: RSSI scan data, whitelist & blacklist.
 */
#include "task_storage.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

blesc_model_rssi_data_t   bleam_rssi_data[APP_CONFIG_MAX_BLEAMS];   /**< RSSI scan data from BLEAMs. */
bleam_ios_raw_whitelist_t ios_raw_whitelist[APP_CONFIG_MAX_BLEAMS]; /**< MAC address whitelist for iOS devices. */
bleam_ios_raw_blacklist_t ios_raw_blacklist[APP_CONFIG_MAX_BLEAMS]; /**< MAC address blacklist for iOS devices. */


/************ Data manipulation and helper functions ************/

void clear_rssi_data(blesc_model_rssi_data_t *data) {
    data->active = 0;
    data->scans_stored_cnt = 0;
    data->timestamp = 0;
    memset(data->bleam_uuid, 0, APP_CONFIG_BLEAM_UUID_SIZE);
    memset(data->mac, 0, BLE_GAP_ADDR_LEN);
    memset(data->raw, 0, 16);
    memset(data->rssi, INT8_MIN, APP_CONFIG_RSSI_PER_MSG);
    memset(data->aoa, 0, APP_CONFIG_RSSI_PER_MSG);
}

blesc_model_rssi_data_t * get_rssi_data(uint8_t index) {
    ASSERT(APP_CONFIG_MAX_BLEAMS > index);
    return &bleam_rssi_data[index];
}

uint32_t how_long_ago(uint32_t past_timestamp) {
#if defined(SDK_15_3)
    return app_timer_cnt_diff_compute(app_timer_cnt_get(), past_timestamp);
#endif
#if defined(SDK_12_3)
    uint32_t diff;
    app_timer_cnt_diff_compute(app_timer_cnt_get(), past_timestamp, &diff);
    return diff;
#endif
}

/************ Save Bleam data ************/

bool app_blesc_save_rssi_to_storage(const uint8_t uuid_storage_index, const uint8_t *rssi, const uint8_t *aoa) {
    VERIFY_PARAM_NOT_NULL(rssi);
    VERIFY_PARAM_NOT_NULL(aoa);
    if (bleam_rssi_data[uuid_storage_index].scans_stored_cnt >= APP_CONFIG_RSSI_PER_MSG) {
        return true;
    }

    bleam_rssi_data[uuid_storage_index].rssi[bleam_rssi_data[uuid_storage_index].scans_stored_cnt] = *rssi;
    bleam_rssi_data[uuid_storage_index].aoa[bleam_rssi_data[uuid_storage_index].scans_stored_cnt] = *aoa;
    bleam_rssi_data[uuid_storage_index].timestamp = app_timer_cnt_get();
    ++bleam_rssi_data[uuid_storage_index].scans_stored_cnt;

    if (APP_CONFIG_RSSI_PER_MSG == bleam_rssi_data[uuid_storage_index].scans_stored_cnt)
        return true;
    else
        return false;
}

uint8_t app_blesc_save_bleam_to_storage(const uint8_t * p_uuid, const uint8_t * p_mac, const uint8_t * p_raw) {
    uint8_t uuid_storage_empty_index = APP_CONFIG_MAX_BLEAMS;
    
    /* Find if received MAC has been scanned/received previously. 
    *  If it wasn't, add new bleam_rssi_data[uuid_storage_empty_index] element */
    for (uint8_t uuid_storage_index = 0; APP_CONFIG_MAX_BLEAMS > uuid_storage_index; ++uuid_storage_index) {
        /* If UUIDs match, it means it was scanned/received before and there is bleam_rssi_data[] element for it */
        if (0 == memcmp(p_uuid, bleam_rssi_data[uuid_storage_index].bleam_uuid, APP_CONFIG_BLEAM_UUID_SIZE)) {
            /* If this MAC was already saved, nothing to do here anymore */
            if (0 == memcmp(p_mac, bleam_rssi_data[uuid_storage_index].mac, BLE_GAP_ADDR_LEN)) {
                return uuid_storage_index;
            }
            /* Save MAC address. */
            for(uint8_t i = 0; BLE_GAP_ADDR_LEN > i; ++i)
                bleam_rssi_data[uuid_storage_index].mac[i] = p_mac[i];
            if(NULL != p_raw) {
                for(uint8_t i = 0; 16 > i; ++i)
                    bleam_rssi_data[uuid_storage_index].raw[i] = p_raw[i];
            }
            bleam_rssi_data[uuid_storage_index].active = 1;
            return uuid_storage_index;
        }
        if (0 == bleam_rssi_data[uuid_storage_index].active) {
            if (uuid_storage_empty_index > uuid_storage_index)
                uuid_storage_empty_index = uuid_storage_index;
            continue;
        }
    }

    if (APP_CONFIG_MAX_BLEAMS == uuid_storage_empty_index) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "STORAGE: Storage is full, can't save new UUID and MAC\n");
        return APP_CONFIG_MAX_BLEAMS;
    }

    /* Save UUID and MAC address */
    for (uint8_t i = 0; APP_CONFIG_BLEAM_UUID_SIZE > i; ++i)
        bleam_rssi_data[uuid_storage_empty_index].bleam_uuid[i] = p_uuid[i];
    for (uint8_t i = 0; BLE_GAP_ADDR_LEN > i; ++i)
        bleam_rssi_data[uuid_storage_empty_index].mac[i] = p_mac[i];
    bleam_rssi_data[uuid_storage_empty_index].active = 1;

    return uuid_storage_empty_index;
}

/************ Whitelist and blacklist ************/

/**@defgroup listing Whitelist and blacklist
 * @brief Whitelist and blacklist management for iOS problem solution.
 * @{
 * @ingroup ios_solution
 */

uint8_t * raw_in_whitelist(const uint8_t * p_raw) {
    uint8_t * res = NULL;
    for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
        if(!ios_raw_whitelist[index].active)
            continue;
        if(NULL != p_raw && 0 == memcmp(ios_raw_whitelist[index].raw, p_raw, 16)) {
            res = ios_raw_whitelist[index].bleam_uuid;
            ios_raw_whitelist[index].timestamp = app_timer_cnt_get();
        } else if(MACLIST_TIMEOUT < how_long_ago(ios_raw_whitelist[index].timestamp)) {
            ios_raw_whitelist[index].active = false;
        }
    }
    return res;
}

ret_code_t add_raw_in_whitelist(const uint8_t * p_raw, uint8_t * p_uuid) {
    if(NULL == p_raw)
        return NRF_ERROR_INVALID_DATA;
    for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
        if(ios_raw_whitelist[index].active)
            continue;
        ios_raw_whitelist[index].active = true;
        memcpy(ios_raw_whitelist[index].raw, p_raw, 16);
        memcpy(ios_raw_whitelist[index].bleam_uuid, p_uuid, APP_CONFIG_BLEAM_UUID_SIZE);
        ios_raw_whitelist[index].timestamp = app_timer_cnt_get();
        return NRF_SUCCESS;
    }
    return NRF_ERROR_NO_MEM;
}

bool raw_in_blacklist(const uint8_t * p_raw) {
    bool res = false;
    for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
        if(!ios_raw_blacklist[index].active)
            continue;
        if(NULL != p_raw && 0 == memcmp(ios_raw_blacklist[index].raw, p_raw, 16)) {
            res = true;
            ios_raw_blacklist[index].timestamp = app_timer_cnt_get();
        } else if(MACLIST_TIMEOUT < how_long_ago(ios_raw_blacklist[index].timestamp))
            ios_raw_blacklist[index].active = false;
    }
    return res;
}

ret_code_t add_raw_in_blacklist(const uint8_t * p_raw) {
    if(NULL == p_raw)
        return NRF_ERROR_INVALID_DATA;
    for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
        if(!ios_raw_whitelist[index].active)
            continue;
        if(0 == memcmp(ios_raw_whitelist[index].raw, p_raw, 16) ||
            MACLIST_TIMEOUT < how_long_ago(ios_raw_whitelist[index].timestamp)) {
            ios_raw_whitelist[index].active = false;
        }
    }
    for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
        if(ios_raw_blacklist[index].active)
            continue;
        ios_raw_blacklist[index].active = true;
        memcpy(ios_raw_blacklist[index].raw, p_raw, 16);
        ios_raw_blacklist[index].timestamp = app_timer_cnt_get();
        return NRF_SUCCESS;
    }
    return NRF_ERROR_NO_MEM;
}

void drop_blacklist(void * p_context) {
    memset(ios_raw_blacklist, 0, APP_CONFIG_MAX_BLEAMS * sizeof(bleam_ios_raw_blacklist_t));
}
/** @} end of ios_solution */

/** @}*/

