/** @file task_scan_connect.c
 *
 * @defgroup task_scan_connect Task Scan-Connect
 * @{
 * @ingroup bleam_connect
 * @ingroup blesc_tasks
 *
 * @brief Scanning and connection to Bleam.
 */
#include "task_scan_connect.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#include "task_bleam.h"
#include "task_board.h"
#include "task_config.h"
#include "task_connect_common.h"
#include "task_time.h"

APP_TIMER_DEF(scan_connect_timer);                      /**< Timer for scan/connect cycle. */
APP_TIMER_DEF(m_eco_timer_id);                          /**< Bleam Scanner sleep/wake cycle timer. */

static ble_db_discovery_t * m_db_disc;                  /**< Bleam discovery module instance. */
static bleam_service_client_t * m_bleam_service_client; /**< Bleam service client instance. */
static nrf_ble_scan_t * m_scan;                         /**< Scanning module instance. */

/** Scanning parameters */
static ble_gap_scan_params_t m_scan_params = {
    .active        = 1,
    .interval      = SCAN_INTERVAL,
    .window        = SCAN_WINDOW,
    .timeout       = SCAN_DURATION,
#if defined(SDK_15_3)
    .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
    .scan_phys     = BLE_GAP_PHY_1MBPS,
#endif
};

static blesc_state_t m_blesc_node_state = BLESC_STATE_SCANNING; /**< Bleam Scanner node current state @ref blesc_state_t */

/** UUID of Bleam service, first and second bytes separately */
const uint16_t uuid_bleam_to_scan[] = {BLEAM_SERVICE_UUID >> 8, BLEAM_SERVICE_UUID & 0x00FF};

/**@brief Structure for storing data for iOS device
 *        that is being investigated to have Bleam running in the background.
 * @ingroup ios_solution
 */
bleam_ios_rssi_data_t stupid_ios_data = {0};  
extern uint16_t m_conn_handle;                /**< Handle of the current connection. */
bool m_bleam_nearby = false;                  /**< Flag that denotes whether a Bleam device has been detected by Bleam Scanner node since latest scan start  */
static uint8_t m_bleam_uuid_index;            /**< Index of Bleam device to connect to in storage */

/************ Data manipulation and helper functions ************/

blesc_state_t blesc_node_state_get(void) {
    return m_blesc_node_state;
}

void blesc_node_state_set(blesc_state_t new_state) {
    m_blesc_node_state = new_state;
}

blesc_model_rssi_data_t * get_connected_bleam_data(void) {
    return get_rssi_data(m_bleam_uuid_index);
}

bool stupid_ios_data_active(void) {
    return stupid_ios_data.active;
}

void stupid_ios_data_clear(void) {
    memset(&stupid_ios_data, 0, sizeof(bleam_ios_rssi_data_t));
}

/**@brief Function for decoding the BLE address type.
 *
 * @param[in] p_addr 	The BLE address.
 *
 * @return    			Address type, or an error if the address type is incorrect, that is it does not match @link_ble_gap_addr_types.
 *
 */
static uint16_t scan_address_type_decode(uint8_t const *p_addr) {
    uint8_t addr_type = p_addr[0];

    // See Bluetooth Core Specification Vol 6, Part B, section 1.3.
    addr_type = addr_type >> 6;
    addr_type &= 0x03;

    switch (addr_type) {
    case 0:
        return BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE;
    case 1:
        return BLE_GAP_ADDR_TYPE_PUBLIC;
    case 2:
        return BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
    case 3:
        return BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    default:
        return BLE_ERROR_GAP_INVALID_BLE_ADDR;
    }
}


/********************* ECO mode handling *********************/

void eco_timer_handler(void * p_context) {
    UNUSED_PARAMETER(p_context);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Eco timer interrupt\r\n");
    switch(m_blesc_node_state) {
    case BLESC_STATE_CONNECT:
        m_blesc_node_state = BLESC_STATE_SCANNING;
    case BLESC_STATE_IDLE:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Eco IDLE -> SCANNING\r\n");
        m_blesc_node_state = BLESC_STATE_SCANNING;
        app_timer_start(m_eco_timer_id, BLESC_SCAN_TIME, NULL);
        // clear old lists
        raw_in_blacklist(NULL);
        raw_in_whitelist(NULL);
        scan_start();
        break;
    case BLESC_STATE_SCANNING:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Eco SCANNING -> IDLE\r\n");
        scan_stop();
        m_blesc_node_state = BLESC_STATE_IDLE;
        // In case Bleam Scanner is going to idle for a long time,
        // make sure it asks for time on next connection
        system_time_needs_update_set();
        break;
    }
}


/********************* Scanning *********************/

/**@brief Function for handling Scanning events.
 *
 * @param[in]     p_scan_evt     Scanning event.
 */
static void scan_evt_handler(scan_evt_t const *p_scan_evt) {
    ret_code_t err_code;

    switch (p_scan_evt->scan_evt_id) {
    case NRF_BLE_SCAN_EVT_CONNECTING_ERROR:
        err_code = p_scan_evt->params.connecting_err.err_code;
        APP_ERROR_CHECK(err_code);
        break;

    case NRF_BLE_SCAN_EVT_CONNECTED: {
        ble_gap_evt_connected_t const *p_connected =
            p_scan_evt->params.connected.p_connected;
        // Scan is automatically stopped by the connection.
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Connecting to target %02x-%02x-%02x-%02x-%02x-%02x\r\n",
            p_connected->peer_addr.addr[5],
            p_connected->peer_addr.addr[4],
            p_connected->peer_addr.addr[3],
            p_connected->peer_addr.addr[2],
            p_connected->peer_addr.addr[1],
            p_connected->peer_addr.addr[0]);
    } break;

    case NRF_BLE_SCAN_EVT_NOT_FOUND:
        process_scan_data(p_scan_evt->params.p_not_found);
        break;
    default:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Scan over event\r\n");
        break;
    }
}

void scan_connect_init(ble_db_discovery_t     * p_db_disc,
                       nrf_ble_scan_t         * p_scan) {
    ret_code_t err_code;
    nrf_ble_scan_init_t init_scan = {
        .p_scan_param = &m_scan_params
    };

    m_db_disc              = p_db_disc;
    m_scan                 = p_scan;

    init_scan.connect_if_match = true;
#if defined(SDK_15_3)
    init_scan.conn_cfg_tag = APP_BLE_CONN_CFG_TAG;
#endif

    err_code = nrf_ble_scan_init(m_scan, &init_scan, scan_evt_handler);
    APP_ERROR_CHECK(err_code);

    // Timer for scan/connect cycle    
    err_code = app_timer_create(&scan_connect_timer, APP_TIMER_MODE_SINGLE_SHOT, scan_connect_timer_handle);
    APP_ERROR_CHECK(err_code);

    // Eco timer.
    err_code = app_timer_create(&m_eco_timer_id, APP_TIMER_MODE_SINGLE_SHOT, eco_timer_handler);
    APP_ERROR_CHECK(err_code);
}

void scan_start(void) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "BLE scanner started\r\n");

    ret_code_t err_code;

    m_blesc_node_state = BLESC_STATE_SCANNING;
    m_bleam_nearby = false;

    err_code = app_timer_start(scan_connect_timer, SCAN_CONNECT_TIME, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_start(m_scan);
    APP_ERROR_CHECK(err_code);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Scanning for UUID %04X\r\n", BLEAM_SERVICE_UUID);

    blesc_toggle_leds(1, 0);
}

void scan_stop(void) {
    nrf_ble_scan_stop();
    app_timer_stop(scan_connect_timer);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Scanning stopped\r\n");
    blesc_toggle_leds(0, 0);
}

/**@brief Function for validating received Bleam device adv data.
 *
 * @param[in] p_data_uuid            Pointer to the advertized UUID.
 * @param[in] rssi_filter_passed     Boolean value of whether the adv report RSSI value is acceptable under RSSI lower level limit.
 *
 * @retval NRF_SUCCESS             If the scanned packet was validated.
 * @retval NRF_ERROR_NOT_FOUND     If 16-bit UUID does not match Bleam UUID.
 * @retval NRF_ERROR_INVALID_PARAM If the scanned device has too low RSSI level.
 * @retval NRF_ERROR_INVALID_ADDR  If this packet was addressed to another specific Bleam Scanner node.
 */
static ret_code_t validate_bleam_adv_report(uint8_t *p_data_uuid, const bool rssi_filter_passed) {
    if (uuid_bleam_to_scan[0] != p_data_uuid[13] || uuid_bleam_to_scan[1] != p_data_uuid[12]) {
        return NRF_ERROR_NOT_FOUND;
    }

    switch (p_data_uuid[11]) {
    case BLEAM_SERVICE_TYPE_AOS:
    case BLEAM_SERVICE_TYPE_IOS:
        if (!rssi_filter_passed)
            return NRF_ERROR_INVALID_PARAM;
        break;
    case BLEAM_SERVICE_TYPE_TOOLS: {
        uint16_t addressee = (((uint16_t)p_data_uuid[9]) << 1) | (uint16_t)p_data_uuid[8];
        if (blesc_node_id_get() != addressee) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "%04X != %04X\r\n", blesc_node_id_get(), addressee);
            return NRF_ERROR_INVALID_ADDR;
        }
        break;
    }}
    return NRF_SUCCESS;
}

/**@brief Function for validating received iOS device adv data.
 * @ingroup ios_solution
 *
 * @param[in] p_data                 Pointer to the possible iOS advertized service structure.
 * @param[in] rssi_filter_passed     Boolean value of whether the adv report RSSI value is acceptable under RSSI lower level limit.
 *
 * @retval NRF_SUCCESS             If the scanned packet was validated.
 * @retval NRF_ERROR_NOT_FOUND     If this data is not from iOS.
 * @retval NRF_ERROR_INVALID_PARAM If the scanned device has too low RSSI level.
 */
static ret_code_t validate_ios_adv_report(uint8_t *p_data, const bool rssi_filter_passed) {
    if (p_data[0] != 0x4C || p_data[1] != 0x00) {
        return NRF_ERROR_NOT_FOUND;
    }

    if (!rssi_filter_passed) {
        return NRF_ERROR_INVALID_PARAM;
    }
    return NRF_SUCCESS;
}

void process_scan_data(ble_gap_evt_adv_report_t const *p_adv_report) {
    ret_code_t err_code;
    uint8_t idx = 0;
    uint16_t dev_name_offset = 0;
    uint16_t field_len;
    data_t adv_data;
    uint32_t time_diff;

    // Initialize advertisement report for parsing
#if defined(SDK_15_3)
    adv_data.p_data = (uint8_t *)p_adv_report->data.p_data;
    adv_data.data_len = p_adv_report->data.len;
#endif
#if defined(SDK_12_3)
    adv_data.p_data = (uint8_t *)p_adv_report->data;
    adv_data.data_len = p_adv_report->dlen;
#endif

    uint8_t p_data_uuid[20] = {0};
    const int8_t rssi_lower_limit = blesc_params_get()->rssi_lower_limit;

    for(uint8_t i = 0; adv_data.data_len > i; ++i) {
        if(adv_data.p_data[i] == 17 &&
            adv_data.data_len >= i + 1 + adv_data.p_data[i] &&
            adv_data.p_data[i+1] == 0x07) {
            memcpy(p_data_uuid, &adv_data.p_data[i+2], adv_data.p_data[i]-1);

            // Show Bleam scans
            err_code = validate_bleam_adv_report(p_data_uuid, rssi_lower_limit < (int8_t)p_adv_report->rssi);
            if(NRF_SUCCESS != err_code) {
                // If found UUID doesn't match Bleam service UUID,
                // continue searching
                i += adv_data.p_data[i];
                continue;
            }

            m_bleam_nearby = true;
            app_timer_stop(m_eco_timer_id);

            uint8_t bleam_uuid_to_send[APP_CONFIG_BLEAM_UUID_SIZE];
            for (int i = 1 + APP_CONFIG_BLEAM_UUID_SIZE, j = 0; i > 1;)
                bleam_uuid_to_send[j++] = p_data_uuid[i--];

            // Save device to storage
            const uint8_t uuid_index = app_blesc_save_bleam_to_storage(bleam_uuid_to_send, p_adv_report->peer_addr.addr, NULL);
            // If storage is full
            if (APP_CONFIG_MAX_BLEAMS == uuid_index) {
                __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Bleam storage full!\r\n");
                return;
            }
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Scanned RSSI %d\r\n", (int8_t)p_adv_report->rssi);
            uint8_t aoa = 0;
            if (app_blesc_save_rssi_to_storage(uuid_index, &p_adv_report->rssi, &aoa)) {
                scan_stop();
                try_bleam_connect(uuid_index);
            }
            return;
        }
        i += adv_data.p_data[i];
    }

    // If Bleam UUID not advertised
    memset(p_data_uuid, 0, 20);
    for (uint8_t i = 0; adv_data.data_len > i; ++i) {
        if (adv_data.p_data[i] == 20 &&
            adv_data.data_len >= i + 1 + adv_data.p_data[i] &&
            adv_data.p_data[i + 1] == 0xFF) {
            memcpy(p_data_uuid, &adv_data.p_data[i + 2], adv_data.p_data[i] - 1);

            err_code = validate_ios_adv_report(p_data_uuid, rssi_lower_limit < (int8_t)p_adv_report->rssi);
            if (NRF_SUCCESS != err_code) {
                i += adv_data.p_data[i];
                continue;
            }
            uint8_t *bleam_uuid_to_send;
            bleam_uuid_to_send = raw_in_whitelist(p_data_uuid + 2);
            // If device is saved
            if (NULL != bleam_uuid_to_send) {
                app_timer_stop(m_eco_timer_id);
                m_bleam_nearby = true;

                const uint8_t uuid_index = app_blesc_save_bleam_to_storage(bleam_uuid_to_send, p_adv_report->peer_addr.addr, p_data_uuid + 2);
                // If storage is full
                if (APP_CONFIG_MAX_BLEAMS == uuid_index) {
                    __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Bleam storage full!\r\n");
                    return;
                }
                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Scanned iOS RSSI %d\r\n", (int8_t)p_adv_report->rssi);
                uint8_t aoa = 0;
                if (app_blesc_save_rssi_to_storage(uuid_index, &p_adv_report->rssi, &aoa)) {
                    scan_stop();
                    try_bleam_connect(uuid_index);
                }
            } else {
                if (raw_in_blacklist(p_data_uuid + 2))
                    return;
                app_timer_stop(m_eco_timer_id);
                stupid_ios_data.active = true;
                memcpy(stupid_ios_data.mac, p_adv_report->peer_addr.addr, BLE_GAP_ADDR_LEN);
                memcpy(stupid_ios_data.raw, p_data_uuid + 2, 16);
                stupid_ios_data.rssi = p_adv_report->rssi;
                stupid_ios_data.aoa = NULL;
                scan_stop();
                try_ios_connect();
            }
            return;
        }
        i += adv_data.p_data[i];
    }
    // Apple twist
    
}


/********************* Connection *********************/

/**@brief Function for handling the scan-connect timer timeout
 */
void scan_connect_timer_handle(void *p_context) {
    if (m_bleam_nearby == false) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "BLESC doesn't see any BLEAMs around.\r\n");
        for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
            clear_rssi_data(get_connected_bleam_data());
        }
        eco_timer_handler(NULL);
        return;
    }

    scan_stop();
    m_blesc_node_state = BLESC_STATE_CONNECT;

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam scan timed out, looking for Bleam to connect.\r\n");

    for(uint8_t index = 0; APP_CONFIG_MAX_BLEAMS > index; ++index) {
        if(get_rssi_data(index)->active) {
            try_bleam_connect(index);
            return;
        }
    }

    // In case there's no BLEAMS in storage, make some
    m_blesc_node_state = BLESC_STATE_SCANNING;
    scan_start();
}

/**@brief Function for initiating a connection to a device
 *
 * @param[in] p_mac      Pointer to MAC address of the device.
 */
static void try_connect(uint8_t * p_mac) {
    ASSERT(NULL != p_mac);

    // If connection is happening right now, we can't connect
    if (BLE_CONN_HANDLE_INVALID != m_conn_handle)
        return;

    m_blesc_node_state = BLESC_STATE_CONNECT;
    ble_gap_addr_t p_ble_gap_addr = {
        .addr_type = scan_address_type_decode(p_mac),
    };
    for (uint8_t i = 0; i < BLE_GAP_ADDR_LEN; ++i) {
        p_ble_gap_addr.addr[i] = p_mac[i];
    }

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC: %02x-%02x-%02x-%02x-%02x-%02x\r\n",
        p_ble_gap_addr.addr[5],
        p_ble_gap_addr.addr[4],
        p_ble_gap_addr.addr[3],
        p_ble_gap_addr.addr[2],
        p_ble_gap_addr.addr[1],
        p_ble_gap_addr.addr[0]);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Addr type: 0x%02X\r\n", p_ble_gap_addr.addr_type);

    if (BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE < p_ble_gap_addr.addr_type) {
        scan_start();
        return;
    }

    // doesn't seem to connect well to public address
    if (BLE_GAP_ADDR_TYPE_PUBLIC == p_ble_gap_addr.addr_type) {
        p_ble_gap_addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    }

    ble_gap_scan_params_t p_scan_params;
    memcpy(&p_scan_params, &(m_scan->scan_params), sizeof(ble_gap_scan_params_t));
    p_scan_params.timeout = CONNECT_TIMEOUT;
    ble_gap_conn_params_t const *p_conn_params = &(m_scan->conn_params);
#if defined(SDK_15_3)
    uint8_t con_cfg_tag = m_scan->conn_cfg_tag;

    ret_code_t err_code = sd_ble_gap_connect(&p_ble_gap_addr,
                                             (ble_gap_scan_params_t const *)(&p_scan_params),
                                             p_conn_params,
                                             con_cfg_tag);
#endif
#if defined(SDK_12_3)
    ret_code_t err_code = sd_ble_gap_connect(&p_ble_gap_addr,
                                             (ble_gap_scan_params_t const *)(&p_scan_params),
                                             p_conn_params);
#endif
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for preparing a connection to Bleam device
 *
 * @details This function replaces base Bleam service UUID Bleam Scanner is about to work with.
 *
 * @param[in] bleam_uuid      Pointer to Bleam service UUID of the Bleam device.
 */
static void prepare_bleam_connect(uint8_t * bleam_uuid) {
    ble_uuid128_t m_bleam_service_base_uuid = {BLE_UUID_BLEAM_SERVICE_BASE_UUID};
    for (uint8_t i = 1 + APP_CONFIG_BLEAM_UUID_SIZE, j = 0; APP_CONFIG_BLEAM_UUID_SIZE > j;) {
        m_bleam_service_base_uuid.uuid128[i--] = bleam_uuid[j++];
    }
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Add new BASE UUID", m_bleam_service_base_uuid.uuid128, 16);
    ret_code_t err_code = bleam_service_uuid_vs_replace(m_bleam_service_client, &m_bleam_service_base_uuid);
    APP_ERROR_CHECK(err_code);
}

void try_bleam_connect(uint8_t p_index) {
    m_bleam_uuid_index = p_index;
    blesc_model_rssi_data_t * bleam_data = get_connected_bleam_data();
    if(NULL == bleam_data) {
        scan_start();
        return;
    }
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "\n\n\nConnecting to Bleam with UUID",
        bleam_data->bleam_uuid, APP_CONFIG_BLEAM_UUID_SIZE);
    prepare_bleam_connect(bleam_data->bleam_uuid);
    try_connect(bleam_data->mac);
}

void try_ios_connect() {
    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG1, "\n\n\nSTUPID Connecting to iOS BLEAM\r\n");
    try_connect(stupid_ios_data.mac);
}


/********************* Handle connection events *********************/

void handle_connect_config(ble_evt_t const *p_ble_evt, nrf_ble_qwr_t * p_qwr) {
    ret_code_t err_code = NRF_SUCCESS;

    m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    err_code = nrf_ble_qwr_conn_handle_assign(p_qwr, m_conn_handle);
    APP_ERROR_CHECK(err_code);
}

void handle_connect_ios(ble_evt_t const *p_ble_evt) {
    m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    bleam_service_discovery_start(m_db_disc, m_conn_handle);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Discovering services on iOS.\r\n");
}

void handle_connect_bleam(ble_evt_t const *p_ble_evt, nrf_ble_qwr_t * p_qwr) {
    ret_code_t err_code = NRF_SUCCESS;

    m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

    err_code = bleam_service_client_handles_assign(m_bleam_service_client, p_ble_evt->evt.gap_evt.conn_handle, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_ble_qwr_conn_handle_assign(p_qwr, m_conn_handle);
    APP_ERROR_CHECK(err_code);

    memset(m_db_disc, 0, sizeof(m_db_disc));
    err_code = ble_db_discovery_start(m_db_disc, m_conn_handle);
    APP_ERROR_CHECK(err_code);
    bleam_inactivity_timer_start();
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Discovering services\r\n");

    blesc_toggle_leds(0, 1);
}

void handle_connection_abort() {
    bleam_connection_abort(m_bleam_service_client);
}

void handle_disconnect(void) {
    if (stupid_ios_data_active())
        stupid_ios_data_clear();
    // clear old lists
    raw_in_whitelist(NULL);
    raw_in_blacklist(NULL);

    scan_start();
}


/********************* Service discovery *********************/

void blesc_services_init(bleam_service_client_t * p_bleam_service_client, void (* cb)(void)) {
    uint32_t err_code;
    bleam_service_client_init_t bleam_init = {0};
    m_bleam_service_client = p_bleam_service_client;

    // Initialize Bleam service.
    bleam_init.evt_handler = bleam_service_evt_handler;
    err_code = bleam_service_client_init(m_bleam_service_client, &bleam_init, cb);
    APP_ERROR_CHECK(err_code);
}

void db_disc_handler(ble_db_discovery_evt_t *p_evt) {
    bleam_service_on_db_disc_evt(m_bleam_service_client, p_evt);
}

void bleam_service_discovery_evt_handler(const bleam_service_discovery_evt_t *p_evt) {
    // Whatever it was, we have to disconnect
    if(p_evt->conn_handle != m_conn_handle)
        return;
    ret_code_t err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if(NRF_ERROR_INVALID_STATE != err_code)
        APP_ERROR_CHECK(err_code);
    // Check if the Bleam Service was discovered on iOS.
    if (p_evt->evt_type == BLEAM_SERVICE_DISCOVERY_COMPLETE && p_evt->params.srv_uuid16.uuid == BLEAM_SERVICE_UUID) {
        if(stupid_ios_data.active) {
            m_bleam_nearby = true;

            for(int i = 1 + APP_CONFIG_BLEAM_UUID_SIZE, j = 0; i > 1;)
                stupid_ios_data.bleam_uuid[j++] = p_evt->params.srv_uuid128.uuid128[i--];
            // Save device to storage
            if(!raw_in_whitelist(stupid_ios_data.raw))
                add_raw_in_whitelist(stupid_ios_data.raw, stupid_ios_data.bleam_uuid);
            const uint8_t uuid_index = app_blesc_save_bleam_to_storage(stupid_ios_data.bleam_uuid, stupid_ios_data.mac, stupid_ios_data.raw);
            // If storage is full
            if (APP_CONFIG_MAX_BLEAMS == uuid_index) {
                __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Bleam storage full!\r\n");
                return;
            }
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Scanned iOS RSSI %d\r\n", (int8_t)stupid_ios_data.rssi);
            uint8_t aoa = 0;
            if(app_blesc_save_rssi_to_storage(uuid_index, &stupid_ios_data.rssi, &aoa)) {
                try_bleam_connect(uuid_index);
            }
        }
    } else if (p_evt->evt_type == BLE_DB_DISCOVERY_SRV_NOT_FOUND ||
               p_evt->evt_type == BLEAM_SERVICE_DISCOVERY_SRV_NOT_FOUND ||
               p_evt->evt_type == BLEAM_SERVICE_DISCOVERY_ERROR) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Wrong iOS device!\r\n");
        if(!raw_in_blacklist(stupid_ios_data.raw))
            add_raw_in_blacklist(stupid_ios_data.raw);
        eco_timer_handler(NULL);
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Unhandled Bleam service discovery event\r\n");
    }
}

/** @}*/