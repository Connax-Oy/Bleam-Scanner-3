#include "app_blesc.h"

#include <stdint.h>
#include <stdlib.h>

#include "sdk_config.h"
#include "example_common.h"
#include "blesc_model.h"
#include "blesc_model_messages.h"

#include "log.h"
#include "app_timer.h"
#include "mesh_main.h"

/** Detected devices' RSSI data storage array */
blesc_model_uuid_rssi_data_t uuid_rssi_data[BLESC_MODEL_BLEAMS_STORAGE_SIZE];

/** Mesh nodes' battery level storage array */
blesc_model_health_data_t health_data[BLESC_MODEL_NODES_STORAGE_SIZE];

uint16_t        health_records_num;                 /**< Number of alive nodes */
static uint16_t m_prime_node_id;                    /**< ID of current elected Prime node */
static uint32_t m_prime_node_last_update_timestamp; /**< Last time Prime node sent its update */

APP_TIMER_DEF(m_prime_node_update_timer);

/**********************  INTERNAL FUNCTIONS  ************************/

/**@brief Function for printing RSSI scan data.
 *
 *@details Function prints received RSSI scan data and its sender ID.
 */
static void print_uuid_rssi_data() {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "\n--- <RSSI data stored> ---\n");
    for (int i = 0; i < BLESC_MODEL_BLEAMS_STORAGE_SIZE; ++i) {
        if (uuid_rssi_data[i].data_array_size > 0) {
            __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "UUID", uuid_rssi_data[i].bleam_uuid, BLESC_MODEL_BLEAM_UUID_SIZE);
            for (int j = 0; uuid_rssi_data[i].data_array_size > j; ++j) {
                blesc_model_rssi_data_t *data = &uuid_rssi_data[i].data[j];
                if (data->scans_stored_cnt != data->scans_mesh_sent_cnt) // if there is something to send to mesh
                    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SENDER_ID: 0x%04x, sent: m %d b %d, stored: %d\r\n",
                        data->sender_id,
                        data->scans_mesh_sent_cnt,
                        data->scans_bleam_sent_cnt,
                        data->scans_stored_cnt);
            }
        }
    }
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "\n--- </RSSI data stored> ---\n");
}

static uint32_t how_long_ago(uint32_t past_timestamp) {
    uint32_t time_diff = log_timestamp_get();
    if (time_diff >= past_timestamp)
        time_diff = time_diff - past_timestamp;
    else
        time_diff = (UINT32_MAX - past_timestamp) + time_diff;
    return time_diff;
}

/**@brief Function for removing dead nodes from and printing health data.
 */
static void clear_and_print_health_data() {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "\n--- <Health data stored> ---\n");
    for (int i = 0; i < BLESC_MODEL_NODES_STORAGE_SIZE; ++i) {
        if (health_data[i].version_id > 0) {
            uint32_t time_diff = how_long_ago(health_data[i].time);
            // if it was more time than APP_CONFIG_BATTERY_LEVEL_INTERVAL plus correction for late message,
            // this node it dead. This function is called every time BLESc receives health message,
            // including the one from itself which is sent every @ref APP_CONFIG_BATTERY_LEVEL_INTERVAL,
            // so the cleaning is done regularly
            if(time_diff > APP_TIMER_TICKS(APP_CONFIG_BATTERY_LEVEL_INTERVAL + 5000)) {
                // node is dead
                health_data[i].version_id = 0;
                --health_records_num;
            }
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Node 0x%04x : %d%% %d secs ago\r\n",
                health_data[i].sender_id, health_data[i].battery_lvl, APP_TIMER_SECS(time_diff));
        }
    }
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "\n--- </Health data stored> ---\n");
}

/**@brief Function for comparing two arrays (UUID or MAC address)
 *
 *@details Function compares received arrays
 *@return  Returns true if arrays match, false if they don't
 *            
 *
 * @param[in] a            Pointer to the array containing the first array.
 * @param[in] b            Pointer to the array containing the second array.
 */
static uint8_t blesc_model_array_match(const uint8_t * a, const uint8_t * b, const uint8_t size) {
    for(int i = 0; i < size; ++i) {
        if(a[i] != b[i]) {
//            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "UUID compare: [%d] %X =!= %X\n", i, UUID_a[i], UUID_b[i]);
            return false;
        }
    }
    return true;
}

/**Comparator function for calculate_rssi_median()
 * 
 * @param[in] a                    Pointer to the first value to compare.
 * @param[in] b                    Pointer to the second value to compare.
 *
 * @returns Returns negative value if the first value is smaller,
 *          positive value if the first value is greater
 *          and zero if values are equal.
*/
int qsort_rssi_comp (const void * a, const void * b) {
    if(*(int8_t *) a > *(int8_t *) b) {
        return 1;
    }
    if(*(int8_t *) a < *(int8_t *) b) {
        return -1;
    }
    return 0;
}

/**Calculates median value for RSSI data array
 * 
 * @param[in] p_rssi                 Pointer to an RSSI scan data array.
 *
 * @returns Returns median value for given array.
 *
*/
static int8_t calculate_rssi_median(int8_t * p_rssi) {
    // sort the p_rssi array with stdlib/qsort
//    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "unsorted RSSI", p_rssi, BLESC_MODEL_RSSI_STORED);
    qsort(p_rssi, BLESC_MODEL_RSSI_STORED, 1, qsort_rssi_comp);
//    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "sorted RSSI", p_rssi, BLESC_MODEL_RSSI_STORED);

    uint8_t k = 0;
    while(k < BLESC_MODEL_RSSI_STORED && p_rssi[k] == BLESC_MODEL_NO_RSSI_DATA)
        ++k;
    return p_rssi[k + ((BLESC_MODEL_RSSI_STORED - k) / 2)];
}

static void prime_node_update_timer_handle(void *p_context) {
    if(app_blesc_is_prime_node()) {
        // send Prime reminder
        mesh_main_send_message(BLESC_MODEL_EVT_PRIME_REMINDER, NULL, NULL);
    } else if(app_blesc_prime_node_exists()) {
        // check if Prime status update is late
        uint32_t time_diff = how_long_ago(m_prime_node_last_update_timestamp);
        if (time_diff > APP_BLESC_PRIME_REELECT_TIMEOUT) {
            // prime node is dead
            m_prime_node_id = 0;
            mesh_app_blesc_reelect_prime();
//            ret_code_t err_code = set_model_publish_address(0x0000, p_server->server[BLESC_MODEL_GENERIC_INDEX].model_handle);
//            APP_ERROR_CHECK(err_code);
        }
    }
}

/*************************** CALLBACKS FOR STORING RECEIVED DATA *****************************/

/**@brief Callback for handling received message from BLESc model
 *
 *@details Function stores received RSSI data in storage.
 * 
 * @param[in] p_self                    Pointer to the BLESc model instance.
 * @param[in] p_meta                    Unused valiable.
 * @param[in] p_in                      Pointer to received RSSI data package.
*/
static void blesc_model_state_on_rssi_data_cb(const blesc_model_server_t * p_self,
                                         const access_message_rx_meta_t * p_meta,
                                         const blesc_model_rssi_params_t * p_in,
                                         uint8_t * uuid_storage_index_out,
                                         uint8_t * data_index_out)
{
//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "received package: SENDER ID: 0x%04x\tBLEAM UUID: %X...\tRSSI: -%d\n", p_in->sender_id, p_in->bleam_uuid[0], p_in->rssi);

    app_blesc_server_t   * p_server = PARENT_BY_FIELD_GET(app_blesc_server_t, server, p_self);

    uint8_t uuid_storage_index = BLESC_MODEL_BLEAMS_STORAGE_SIZE;
    uint8_t uuid_storage_empty_index = BLESC_MODEL_BLEAMS_STORAGE_SIZE;
    uint8_t data_index = BLESC_MODEL_NODES_STORAGE_SIZE;

    /* Find if received UUID has been scanned/received previously. 
    *  If it was, update existing uuid_rssi_data[] element */
    for (uuid_storage_index = 0; BLESC_MODEL_BLEAMS_STORAGE_SIZE > uuid_storage_index; ++uuid_storage_index) {
        if (0 == uuid_rssi_data[uuid_storage_index].data_array_size) {
            if (uuid_storage_empty_index > uuid_storage_index)
                uuid_storage_empty_index = uuid_storage_index;
            continue;
        }
        /* If UUIDs match, it means it was scanned/received before and there is uuid_rssi_data[] element for it */
        if (blesc_model_array_match(p_in->bleam_uuid, uuid_rssi_data[uuid_storage_index].bleam_uuid, BLESC_MODEL_BLEAM_UUID_SIZE)) {
            break;
        }
    }

    /* If UUID was scanned/received earlier, find data index */
    if (BLESC_MODEL_BLEAMS_STORAGE_SIZE > uuid_storage_index) {
        /* Find if this UUID has been received from this sender before,
         * if it was, find the data[] array index for this sender ID,
         * otherwise use empty data element */
        for (data_index = 0; uuid_rssi_data[uuid_storage_index].data_array_size > data_index; ++data_index) {
            /* If sender IDs match, it means this sender has sent data for this UUID before */
            if (p_in->sender_id == uuid_rssi_data[uuid_storage_index].data[data_index].sender_id) {
                break;
            }
        }

        /* If this sender hadn't sent data before, add a data element */
        if (uuid_rssi_data[uuid_storage_index].data_array_size == data_index) {
            if (BLESC_MODEL_NODES_STORAGE_SIZE == uuid_rssi_data[uuid_storage_index].data_array_size) {
                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "STORAGE: Storage is full, can't save data from this sender\n");
                print_uuid_rssi_data();
                return;
            } else {
                data_index = uuid_rssi_data[uuid_storage_index].data_array_size++;

                /* Fill data for this empty entry */
                uuid_rssi_data[uuid_storage_index].data[data_index].sender_id            = p_in->sender_id;
                uuid_rssi_data[uuid_storage_index].data[data_index].scans_stored_cnt     = 0;
                uuid_rssi_data[uuid_storage_index].data[data_index].scans_mesh_sent_cnt  = 0;
                uuid_rssi_data[uuid_storage_index].data[data_index].scans_bleam_sent_cnt = 0;
            }
        }
    }
    /* If UUID wasn't received earlier, fill empty entry */
    else {
        if (BLESC_MODEL_BLEAMS_STORAGE_SIZE == uuid_storage_empty_index) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "STORAGE: Storage is full, can't save new UUID\n");
            print_uuid_rssi_data();
            return;
        } else {
            uuid_storage_index = uuid_storage_empty_index;
            data_index = 0;

            clear_rssi_data(uuid_rssi_data[uuid_storage_index].data);

            /* Fill data for this empty entry */
            uuid_rssi_data[uuid_storage_index].data_array_size = 1;
            for(int i = 0; i < BLESC_MODEL_BLEAM_UUID_SIZE; ++i) {
                uuid_rssi_data[uuid_storage_index].bleam_uuid[i] = p_in->bleam_uuid[i];
            }
            uuid_rssi_data[uuid_storage_index].data[data_index].sender_id            = p_in->sender_id;
            uuid_rssi_data[uuid_storage_index].data[data_index].scans_stored_cnt     = 0;
            uuid_rssi_data[uuid_storage_index].data[data_index].scans_mesh_sent_cnt  = 0;
            uuid_rssi_data[uuid_storage_index].data[data_index].scans_bleam_sent_cnt = 0;
        }
    }
    /* A result from the code above are coordinates of the element to be updated:
       uuid_rssi_data[ uuid_storage_index ].data[ data_index ] */
    blesc_model_rssi_data_t *data = &uuid_rssi_data[uuid_storage_index].data[data_index];
    *uuid_storage_index_out = uuid_storage_index;
    *data_index_out = data_index;

    /* Add RSSI and AOA data to data array entry */

    for (uint8_t j = 0; j < BLESC_MODEL_RSSI_PER_MSG; ++j) {
        if(BLESC_MODEL_NO_RSSI_DATA != p_in->rssi[j]) {
            data->rssi[data->scans_stored_cnt] = p_in->rssi[j];
            data->aoa[data->scans_stored_cnt]  = p_in->aoa[j];
            data->scans_stored_cnt = (data->scans_stored_cnt + 1) % BLESC_MODEL_RSSI_STORED;
            if(data->scans_stored_cnt == data->scans_mesh_sent_cnt) {
                data->scans_mesh_sent_cnt = (data->scans_mesh_sent_cnt + 1) % BLESC_MODEL_RSSI_STORED; 
            }
            if(data->scans_stored_cnt == data->scans_bleam_sent_cnt) {
                data->scans_bleam_sent_cnt = (data->scans_bleam_sent_cnt + 1) % BLESC_MODEL_RSSI_STORED; 
            }
        }
    }

    print_uuid_rssi_data();
}

/**@brief Callback for handling received health message from BLESc model
 *
 *@details Function stores received health data in storage.
 * 
 * @param[in] p_self                    Pointer to the BLESc model instance.
 * @param[in] p_meta                    Unused valiable.
 * @param[in] p_in                      Pointer to received health data package.
*/
static void blesc_model_state_on_health_cb(const blesc_model_server_t * p_self,
                                           const access_message_rx_meta_t * p_meta,
                                           const blesc_model_event_params_t * p_in,
                                           uint8_t * data_index_out)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Health message: SENDER ID: 0x%04x\tBATTERY %: %d...\n", p_in->sender_id, p_in->data[0]);

    app_blesc_server_t *p_server = PARENT_BY_FIELD_GET(app_blesc_server_t, server, p_self);

    app_blesc_save_health_to_storage(p_in->sender_id, p_in->data[0], p_in->data[1]);
    clear_and_print_health_data();
}

/**@brief Callback for handling received test/default message from BLESc model
 *
 *@details Function stores received health data in storage.
 * 
 * @param[in] p_self                    Pointer to the BLESc model instance.
 * @param[in] p_meta                    Unused valiable.
 * @param[in] p_in                      Pointer to received health data package.
*/
static bool blesc_model_state_on_prime_cb(const blesc_model_server_t * p_self,
                                           const access_message_rx_meta_t * p_meta,
                                           const blesc_model_event_params_t * p_in,
                                           uint8_t * data_index_out)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Prime reminder: SENDER ID: 0x%04x\r\n", p_in->sender_id);

    app_blesc_server_t *p_server = PARENT_BY_FIELD_GET(app_blesc_server_t, server, p_self);
    ret_code_t err_code = NRF_SUCCESS;

    m_prime_node_last_update_timestamp = log_timestamp_get();
    if(p_in->sender_id != m_prime_node_id) {
        // New Prime node elected: set new publish address for Generic model
        m_prime_node_id = p_in->sender_id;
        err_code = set_generic_model_publish_address(m_prime_node_id);
        APP_ERROR_CHECK(err_code);
        return true;
    }

    return false;
}

/**@brief Callback for packing an RSSI message for BLESc model from storage
 * 
 * @param[in,out] msg_pkt                    Pointer to the message buffer.
 * @param[in]     uuid_storage_index         Index for scan data for a particular BLEAM.
 * @param[in]     data_index                 Index for a particular sender's scan data for aforementioned BLEAM.
*/
static uint32_t rssi_msg_from_storage_cb(blesc_model_rssi_msg_pkt_t * p_rssi, uint8_t uuid_storage_index, uint8_t data_index) {
    blesc_model_rssi_data_t * data = &uuid_rssi_data[uuid_storage_index].data[data_index];

    if(0x0000 == data->sender_id) {
        return NRF_ERROR_NOT_FOUND;
    }

    p_rssi->sender_id = data->sender_id;
    for (uint8_t i = 0; i < BLESC_MODEL_BLEAM_UUID_SIZE; ++i)
        p_rssi->bleam_uuid[i] = uuid_rssi_data[uuid_storage_index].bleam_uuid[i];

    uint8_t sent   = uuid_rssi_data[uuid_storage_index].data[data_index].scans_mesh_sent_cnt;
    uint8_t stored = uuid_rssi_data[uuid_storage_index].data[data_index].scans_stored_cnt;
//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "sent: %d, stored: %d\r\n", sent, stored);
    if(sent > stored)
        stored += BLESC_MODEL_RSSI_STORED;
    if(BLESC_MODEL_RSSI_PER_MSG > stored - sent) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    for (uint8_t i = 0; sent < stored && i < BLESC_MODEL_RSSI_PER_MSG; ++sent, ++i) {
        p_rssi->rssi[i] = data->rssi[sent % BLESC_MODEL_RSSI_STORED];
        p_rssi->aoa[i]  = data->aoa[sent % BLESC_MODEL_RSSI_STORED];
    }

//    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Assembled msg", (uint8_t *)p_rssi, BLESC_MODEL_RSSI_MSG_LEN);
    return NRF_SUCCESS;
}

static void rssi_mark_mesh_sent_cb(uint8_t uuid_storage_index, uint8_t data_index) {
    blesc_model_rssi_data_t * data = &uuid_rssi_data[uuid_storage_index].data[data_index];
    data->scans_mesh_sent_cnt = (data->scans_mesh_sent_cnt + BLESC_MODEL_RSSI_PER_MSG) % BLESC_MODEL_RSSI_STORED;
}

/**@brief Callback for packing a health message for BLESc model from storage
 *
 *@details Function stores received health data in storage.
 * 
 * @param[in,out] msg_pkt                    Pointer to the message buffer.
 * @param[in]     data_index                 Index for a particular sender's health
*/
static uint32_t health_msg_from_storage(blesc_model_event_msg_pkt_t * p_health, uint8_t data_index) {
    blesc_model_health_data_t * data = &health_data[data_index];

    if(0x0000 == data->sender_id) {
        return NRF_ERROR_NOT_FOUND;
    }

    p_health->sender_id = data->sender_id;
    p_health->event     = BLESC_MODEL_EVT_HEALTH_STATUS;
    p_health->data[0]   = data->battery_lvl;
    p_health->data[1]   = data->version_id;

    return NRF_SUCCESS;
}

const blesc_model_server_callbacks_t blesc_cbs = {
    .blesc_cbs.on_rssi_data_cb = blesc_model_state_on_rssi_data_cb,
    .blesc_cbs.on_health_cb = blesc_model_state_on_health_cb,
    .blesc_cbs.on_prime_cb = blesc_model_state_on_prime_cb,
    .blesc_cbs.node_address_cb = mesh_get_node_address,
    .blesc_cbs.is_prime_node_cb = app_blesc_is_prime_node,
    .blesc_cbs.rssi_msg_from_storage_cb = rssi_msg_from_storage_cb,
    .blesc_cbs.rssi_mark_mesh_sent_cb = rssi_mark_mesh_sent_cb,
    .blesc_cbs.health_msg_from_storage_cb = health_msg_from_storage,
};

/************************* Interface functions ************************/

void clear_rssi_data(blesc_model_rssi_data_t *data) {
    for (int i = 0; i < BLESC_MODEL_NODES_STORAGE_SIZE; ++i) {
        data[i].sender_id            = 0;
        data[i].scans_stored_cnt     = 0;
        data[i].scans_mesh_sent_cnt  = 0;
        data[i].scans_bleam_sent_cnt = 0;
        for (int j = 0; j < BLESC_MODEL_RSSI_STORED; ++j) {
            data[i].rssi[j] = BLESC_MODEL_NO_RSSI_DATA;
            data[i].aoa[j] = 0;
        }
    }
}

ret_code_t app_blesc_reduce_my_rssi_scans_amount(blesc_model_rssi_data_t *data, uint8_t scans_to_remain) {
    for (int i = 0; i < BLESC_MODEL_NODES_STORAGE_SIZE; ++i) {
        if(mesh_get_node_address() != data[i].sender_id)
            continue;
        uint8_t scans_to_send = data->scans_stored_cnt - data->scans_bleam_sent_cnt;
        if(data->scans_stored_cnt < data->scans_bleam_sent_cnt)
            scans_to_send += BLESC_MODEL_RSSI_STORED;
        if(scans_to_remain > scans_to_send)
            return NRF_ERROR_INVALID_DATA;
        if(scans_to_remain < scans_to_send) {
            data->scans_bleam_sent_cnt = (data->scans_bleam_sent_cnt + scans_to_send - scans_to_remain) % BLESC_MODEL_RSSI_STORED;
            data->scans_mesh_sent_cnt = data->scans_bleam_sent_cnt;
        }
        return NRF_SUCCESS;
    }
    return NRF_ERROR_NOT_FOUND;
}

void app_blesc_basic_init() {
    uint32_t status = NRF_SUCCESS;

    /* Clear data storage structure */
    for (uint8_t i = 0; i < BLESC_MODEL_BLEAMS_STORAGE_SIZE; ++i) {
        uuid_rssi_data[i].data_array_size = 0;
        memset(uuid_rssi_data[i].bleam_uuid, 0, BLESC_MODEL_BLEAM_UUID_SIZE);
        memset(uuid_rssi_data[i].mac, 0, BLE_GAP_ADDR_LEN);
        clear_rssi_data(uuid_rssi_data[i].data);
    }
    health_records_num = 0;

    status = app_timer_create(&m_prime_node_update_timer, APP_TIMER_MODE_REPEATED, prime_node_update_timer_handle);
    APP_ERROR_CHECK(status);

    status = app_timer_start(m_prime_node_update_timer, APP_BLESC_PRIME_SEND_TIMEOUT, NULL);
    APP_ERROR_CHECK(status);
}

uint32_t app_blesc_init(app_blesc_server_t * p_server, uint8_t element_index) {
    uint32_t status = NRF_ERROR_INTERNAL;

    if (p_server == NULL)
    {
        return NRF_ERROR_NULL;
    }

    p_server->server[element_index].settings.p_callbacks = &blesc_cbs;

    switch (element_index) {
    case BLESC_MODEL_GENERIC_INDEX:
        status = blesc_model_generic_server_init(&p_server->server[element_index], element_index + 1);
        break;
    case BLESC_MODEL_PRIME_INDEX:
        status = blesc_model_prime_server_init(&p_server->server[element_index], element_index + 1);
        break;
    }
    return status;
}

ret_code_t app_blesc_find_bleam_to_connect(uint16_t node_address, uint8_t * bleam_uuid_index) {
    uint8_t rssi_array[BLESC_MODEL_RSSI_STORED];
    int8_t max_rssi_median;
    uint16_t sender_id;

    // Loop through all UUIDs this node has scanned
    for(int uuid_index = 0; BLESC_MODEL_BLEAMS_STORAGE_SIZE > uuid_index; ++uuid_index) {
        // If data entry is empty, skip
        if(!uuid_rssi_data[uuid_index].data_array_size)
            continue;
        for(int sender_index = 0; uuid_rssi_data[uuid_index].data_array_size > sender_index; ++sender_index) {
            // If no RSSI data from this node, skip
            if(uuid_rssi_data[uuid_index].data[sender_index].sender_id != node_address)
                continue;
            ret_code_t err_code = app_blesc_reduce_my_rssi_scans_amount(uuid_rssi_data[uuid_index].data,
                                                                        APP_CONFIG_SCANS_BEFORE_CONNECT);
            // If not enough RSSI data, skip
            if(NRF_SUCCESS != err_code) {
                continue;
            }
            *bleam_uuid_index = uuid_index;
            return NRF_SUCCESS;
        }
    }
    return NRF_ERROR_NOT_FOUND;
}

uint8_t app_blesc_save_bleam_to_storage(const uint8_t * p_uuid, const uint8_t * p_mac) {
    uint8_t uuid_storage_empty_index = BLESC_MODEL_BLEAMS_STORAGE_SIZE;
    
    /* Find if received MAC has been scanned/received previously. 
    *  If it wasn't, add new uuid_rssi_data[uuid_storage_empty_index] element */
    for (uint8_t uuid_storage_index = 0; BLESC_MODEL_BLEAMS_STORAGE_SIZE > uuid_storage_index; ++uuid_storage_index) {
        /* If UUIDs match, it means it was scanned/received before and there is uuid_rssi_data[] element for it */
        if (blesc_model_array_match(p_uuid, uuid_rssi_data[uuid_storage_index].bleam_uuid, BLESC_MODEL_BLEAM_UUID_SIZE)) {
            /* If this MAC was already saved, nothing to do here anymore */
            if (blesc_model_array_match(p_mac, uuid_rssi_data[uuid_storage_index].mac, BLE_GAP_ADDR_LEN)) {
                return uuid_storage_index;
            }
            /* Save MAC address. */
            for(uint8_t i = 0; BLE_GAP_ADDR_LEN > i; ++i)
                uuid_rssi_data[uuid_storage_index].mac[i] = p_mac[i];
            return uuid_storage_index;
        }
        if (0 == uuid_rssi_data[uuid_storage_index].data_array_size) {
            if (uuid_storage_empty_index > uuid_storage_index)
                uuid_storage_empty_index = uuid_storage_index;
            continue;
        }
    }

    if (BLESC_MODEL_BLEAMS_STORAGE_SIZE == uuid_storage_empty_index) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "STORAGE: Storage is full, can't save new UUID and MAC\n");
        print_uuid_rssi_data();
        return BLESC_MODEL_BLEAMS_STORAGE_SIZE;
    }

    /* Save UUID and MAC address */
    for(uint8_t i = 0; BLESC_MODEL_BLEAM_UUID_SIZE > i; ++i)
        uuid_rssi_data[uuid_storage_empty_index].bleam_uuid[i] = p_uuid[i];
    for(uint8_t i = 0; BLE_GAP_ADDR_LEN > i; ++i)
        uuid_rssi_data[uuid_storage_empty_index].mac[i] = p_mac[i];

    return uuid_storage_empty_index;
}

uint8_t app_blesc_save_rssi_to_storage(const uint8_t uuid_storage_index, const uint8_t * rssi, const uint8_t * aoa, uint8_t *p_scans_to_send_bleam) {
    uint8_t data_index;

    for (data_index = 0; uuid_rssi_data[uuid_storage_index].data_array_size > data_index; ++data_index) {
        /* If sender IDs match, it means this sender has sent data for this UUID before */
        if (mesh_get_node_address() == uuid_rssi_data[uuid_storage_index].data[data_index].sender_id) {
            break;
        }
    }

    /* If this sender hadn't sent data before, add a data element */
    if (uuid_rssi_data[uuid_storage_index].data_array_size == data_index) {
        data_index = uuid_rssi_data[uuid_storage_index].data_array_size++;
        uuid_rssi_data[uuid_storage_index].data[data_index].sender_id = mesh_get_node_address();

        uuid_rssi_data[uuid_storage_index].data[data_index].scans_stored_cnt     = 0;
        uuid_rssi_data[uuid_storage_index].data[data_index].scans_mesh_sent_cnt  = 0;
        uuid_rssi_data[uuid_storage_index].data[data_index].scans_bleam_sent_cnt = 0;
    }

    blesc_model_rssi_data_t *data = &uuid_rssi_data[uuid_storage_index].data[data_index];

    /* Add RSSI and AOA data to data array entry */
    data->scans_stored_cnt             %= BLESC_MODEL_RSSI_STORED;
    data->rssi[data->scans_stored_cnt] = *rssi;
    data->aoa[data->scans_stored_cnt] = *aoa;
    data->scans_stored_cnt = (data->scans_stored_cnt + 1) % BLESC_MODEL_RSSI_STORED;
    if (data->scans_stored_cnt == data->scans_mesh_sent_cnt) {
        data->scans_mesh_sent_cnt = (data->scans_mesh_sent_cnt + 1) % BLESC_MODEL_RSSI_STORED;
    }
    if (data->scans_stored_cnt == data->scans_bleam_sent_cnt) {
        data->scans_bleam_sent_cnt = (data->scans_bleam_sent_cnt + 1) % BLESC_MODEL_RSSI_STORED;
    }

    uint8_t scans_to_send = data->scans_stored_cnt - data->scans_mesh_sent_cnt;
    if(data->scans_stored_cnt < data->scans_mesh_sent_cnt)
        scans_to_send += BLESC_MODEL_RSSI_STORED;

    if(NULL != p_scans_to_send_bleam) {
        *p_scans_to_send_bleam = data->scans_stored_cnt - data->scans_bleam_sent_cnt;
        if(data->scans_stored_cnt < data->scans_bleam_sent_cnt)
            *p_scans_to_send_bleam += BLESC_MODEL_RSSI_STORED;
    }
 
    if (BLESC_MODEL_RSSI_PER_MSG <= scans_to_send)
        return data_index;
    else
        return BLESC_MODEL_NODES_STORAGE_SIZE;
}

uint8_t app_blesc_save_health_to_storage(uint16_t sender_id, uint8_t battery_lvl, uint8_t version_id) {
    uint8_t data_index;
    uint8_t data_index_empty = BLESC_MODEL_NODES_STORAGE_SIZE;

    /* Find if health data has been received from this sender before,
     * if it was, find the data[] array index for this sender ID,
     * otherwise use empty data element */
    for (data_index = 0; BLESC_MODEL_NODES_STORAGE_SIZE > data_index; ++data_index) {
        /* If sender IDs match, it means this sender has sent data for this UUID before */
        if (sender_id == health_data[data_index].sender_id) {
            break;
        }
        if(data_index_empty == BLESC_MODEL_NODES_STORAGE_SIZE && 0 == health_data[data_index].sender_id) {
            data_index_empty = data_index;
        }
    }

    /* If this sender hadn't sent data before, add a data element */
    if (BLESC_MODEL_NODES_STORAGE_SIZE == data_index) {
        if (BLESC_MODEL_NODES_STORAGE_SIZE == data_index_empty) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "STORAGE: Health storage is full, can't save data from this sender\n");
            return BLESC_MODEL_NODES_STORAGE_SIZE;
        } else {
            data_index = data_index_empty;
            ++health_records_num;

            /* Fill data for this empty entry */
            health_data[data_index].sender_id = sender_id;
        }
    }

    health_data[data_index].battery_lvl = battery_lvl;
    health_data[data_index].version_id  = version_id;
    health_data[data_index].time        = log_timestamp_get();

    return data_index;
}

bool app_blesc_prime_node_exists(void) {
    return (0 != m_prime_node_id);
}

bool app_blesc_is_prime_node(void) {
    return (app_blesc_prime_node_exists() && (mesh_get_node_address() == m_prime_node_id));
}

ret_code_t app_blesc_set_this_node_as_prime(void) {
    uint16_t node_addr = mesh_get_node_address();
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Node addr: 0x%04x, Prime addr: 0x%04x\r\n", node_addr, m_prime_node_id);
    if(NULL == node_addr)
        return NRF_ERROR_INVALID_ADDR;
    if(0 != m_prime_node_id)
        return NRF_ERROR_INVALID_STATE;
    m_prime_node_id = node_addr;
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "This node is now in Prime mode\r\n");
    return NRF_SUCCESS;
}

ret_code_t app_blesc_set_this_node_as_generic(void) {
    uint16_t node_addr = mesh_get_node_address();
    if(NULL == node_addr)
        return NRF_ERROR_INVALID_ADDR;
    if(node_addr != m_prime_node_id) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "This node was already in Generic mode\r\n");
        return NRF_ERROR_INVALID_STATE;
    }
    m_prime_node_id = 0;
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "This node is now in Generic mode\r\n");
    return NRF_SUCCESS;
}