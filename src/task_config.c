/** @file task_config.c
 *
 * @defgroup task_config Task Config
 * @{
 * @ingroup blesc_config
 * @ingroup blesc_tasks
 *
 * @brief Configuration intitialisation and event handler.
 */
#include "task_config.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#if defined(SDK_15_3)
#include "nrf_sdh.h"
#endif
#if defined(SDK_12_3)
#include "softdevice_handler.h"
#endif
#include "ble_advdata.h"

#include "task_connect_common.h"

extern configuration_t m_blesc_config; /**< Bleam Scanner configuration data, extern from task_fds.h */
extern blesc_params_t  m_blesc_params; /**< Bleam Scanner params, extern from task_fds.h */

/** Detailed configuration fail status. */
static config_s_fail_status_t m_blesc_status_fail_detailed;

/** Configuration Servire server instance. */
static config_s_server_t * m_config_s_server = NULL;

__ALIGN(4) static uint8_t m_blesc_public_key[BLESC_PUBLIC_KEY_SIZE]; /**< Bleam Scanner public key */
static uint8_t            m_chunks_cnt;                              /**< Number of data chunks sent counter */

/********************* Advertising *********************/

#if defined(SDK_15_3)
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET; /**< Advertising handle used to identify an advertising set. */
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];  /**< Buffer for storing an encoded advertising set. */
static ble_gap_adv_data_t m_adv_data = {                      
    .adv_data = {
        .p_data = m_enc_advdata,
        .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
     }
}; /**< Struct that contains pointers to the encoded advertising data. */
#endif
static ble_gap_adv_params_t m_adv_params; /**< Advertising params. */

/**@brief Function for starting advertising.
 *
 * @returns Nothing.
 */
static void advertising_start(void) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam Scanner is waiting to be configured.\r\n");
    ret_code_t err_code = NRF_SUCCESS;
#if defined(SDK_15_3)
    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
#endif
#if defined(SDK_12_3)
    err_code = sd_ble_gap_adv_start(&m_adv_params);
#endif
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for stopping advertising.
 *
 * @returns Nothing.
 */
static void advertising_stop(void) {   
#if defined(SDK_15_3) 
    sd_ble_gap_adv_stop(m_adv_handle);
#endif
#if defined(SDK_12_3)
    sd_ble_gap_adv_stop();
#endif
}

/**@brief Function for initializing the Advertising functionality.
 *
 * @returns Nothing.
 */
void advertising_init(void) {
    ret_code_t    err_code;
    ble_advdata_t advdata     = {0};
    ble_uuid_t    adv_uuids[] = {{CONFIG_S_UUID, BLE_UUID_TYPE_VENDOR_BEGIN}};

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
//    advdata.include_appearance    = true;
    advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = adv_uuids;

#if defined(SDK_15_3)
    err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
#endif
#if defined(SDK_12_3)
    err_code = ble_advdata_set(&advdata, NULL);
#endif
    APP_ERROR_CHECK(err_code);

    // Set advertising parameters.
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.p_peer_addr     = NULL;
    m_adv_params.interval        = APP_ADV_INTERVAL;

#if defined(SDK_15_3)
    m_adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
    m_adv_params.duration        = APP_ADV_DURATION;
    m_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    m_adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &m_adv_params);
    APP_ERROR_CHECK(err_code);
#endif

    advertising_start();
}

/*********** Configuration Service partial handlers ********/

/**@brief Function for resetting Bleam Scanner node after successful configuration.
 *
 * @returns Nothing.
 */
static void leave_config_mode(void) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam Scanner is leaving config mode.\r\n");
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "====== System restart ======\r\n");
    sd_nvic_SystemReset();
}

/**@brief Function for handling a pubkey chunk received by Configuration Service.
 *
 * @param[in] p_config_s_server       Pointer to Configuration Service instance.
 * @param[in] p_evt_write             Pointer to GATT Write event.
 *
 * @returns Nothing.
 */
static void config_service_on_bleam_pubkey(config_s_server_t *p_config_s_server, ble_gatts_evt_write_t const *p_evt_write) {
    ret_code_t err_code = NRF_SUCCESS;
    if (CONFIG_S_STATUS_WAITING == config_s_get_status()) {
        uint8_t chunk_number = p_evt_write->data[0];

        // Check if chunk number is valid
        if (0 == chunk_number || BLESC_PUBLIC_KEY_SIZE / APP_CONFIG_DATA_CHUNK_SIZE < chunk_number) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received invalid pubkey chunk number.\r\n");
            m_blesc_status_fail_detailed = CONFIG_S_FAIL_DATA;
            config_status_update(CONFIG_S_STATUS_FAIL);
            return;
        }
        recvd_chunks_add(chunk_number - 1);
        memcpy(m_blesc_config.keys.bleam_public_key + (APP_CONFIG_DATA_CHUNK_SIZE * (chunk_number - 1)), p_evt_write->data + 1, APP_CONFIG_DATA_CHUNK_SIZE);
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received chunk %u of Bleam public key\r\n", chunk_number);
        if (recvd_chunks_validate(BLESC_PUBLIC_KEY_SIZE / APP_CONFIG_DATA_CHUNK_SIZE)) {
#ifdef SDK_12_3
            reverse_array_in_32_byte_chunks(m_blesc_config.keys.bleam_public_key, BLESC_PUBLIC_KEY_SIZE);
#endif
            __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Set Bleam public key to", m_blesc_config.keys.bleam_public_key, BLESC_PUBLIC_KEY_SIZE);
            // If node ID was set already
            if (0 != m_blesc_config.node_id) {
                if (NRF_SUCCESS != flash_config_write()) {
                    m_blesc_status_fail_detailed = CONFIG_S_FAIL_FDS;
                    config_status_update(CONFIG_S_STATUS_FAIL);
                }
            }
        }
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Reset flash to set new keys and node ID.\r\n");
    }
}

/**@brief Function for handling a node ID received by Configuration Service.
 *
 * @param[in] p_config_s_server       Pointer to Configuration Service instance.
 * @param[in] p_evt_write             Pointer to GATT Write event.
 *
 * @returns Nothing.
 */
static void config_service_on_node_id(config_s_server_t *p_config_s_server, ble_gatts_evt_write_t const *p_evt_write) {
    ret_code_t err_code = NRF_SUCCESS;
    if (CONFIG_S_STATUS_WAITING == config_s_get_status()) {
        m_blesc_config.node_id = ((uint16_t)p_evt_write->data[0] << 8) | ((uint16_t)p_evt_write->data[1]);

        // Check if received node ID is valid
        if(0 == m_blesc_config.node_id) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received invalid node ID.\r\n");
            m_blesc_status_fail_detailed = CONFIG_S_FAIL_DATA;
            config_status_update(CONFIG_S_STATUS_FAIL);
            return;
        }

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Set node ID to %04X\r\n", m_blesc_config.node_id);
        if (NRF_SUCCESS != flash_config_write()) {
            m_blesc_status_fail_detailed = CONFIG_S_FAIL_FDS;
            config_status_update(CONFIG_S_STATUS_FAIL);
        }
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Reset flash to set new keys and node ID.\r\n");
    }
}

/**@brief Function for handling STATUS char value change.
 *
 * @param[in] p_config_s_server       Pointer to Configuration Service instance.
 * @param[in] p_evt_write             Pointer to GATT Write event.
 *
 * @returns Nothing.
 */
static void config_service_on_recv_status(config_s_server_t *p_config_s_server, ble_gatts_evt_write_t const *p_evt_write) {
    ret_code_t err_code = NRF_SUCCESS;
    if (CONFIG_S_STATUS_SET == config_s_get_status() && CONFIG_S_STATUS_DONE == p_evt_write->data[0]) {
        // Configuration finished
        config_s_finish();
        err_code = sd_ble_gap_disconnect(p_config_s_server->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if (NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
        leave_config_mode();
    } else if (CONFIG_S_STATUS_FAIL == p_evt_write->data[0]) {
        // Check if configuration failed externally
        if(CONFIG_S_NO_FAIL == m_blesc_status_fail_detailed)
            m_blesc_status_fail_detailed = CONFIG_S_FAIL_BLEAM;
        config_s_server_evt_t evt = {
            .evt_type = CONFIG_S_SERVER_EVT_FAIL,
        };
        config_s_event_handler(p_config_s_server, &evt);
    }
}

/**@brief Function for handling a Bleam Scanner pubkey char subscription by Configuration Service.
 *
 * @param[in] p_config_s_server       Pointer to Configuration Service instance.
 *
 * @returns Nothing.
 */
static void config_service_on_sub_blesc_pubkey(config_s_server_t *p_config_s_server) {
    ret_code_t err_code = NRF_SUCCESS;

    generate_blesc_keys(m_blesc_config.keys.blesc_private_key, m_blesc_public_key);

    // Send the first chunk
    m_chunks_cnt = 1;
    err_code = config_s_publish_pubkey_chunk(p_config_s_server, m_chunks_cnt, m_blesc_public_key);
    if (err_code != NRF_ERROR_INVALID_STATE) {
        APP_ERROR_CHECK(err_code);
    }
}

void config_s_event_handler(config_s_server_t *p_config_s_server, config_s_server_evt_t const *p_evt) {
    ret_code_t err_code;

    switch (p_evt->evt_type) {

    case CONFIG_S_SERVER_EVT_CONNECTED:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Connected\r\n");
        m_chunks_cnt = 0;
        recvd_chunks_clear();
        m_blesc_status_fail_detailed = CONFIG_S_NO_FAIL;
        config_s_publish_version(p_config_s_server);
        bleam_inactivity_timer_start();
        break;

    case CONFIG_S_SERVER_EVT_DISCONNECTED:
        bleam_inactivity_timer_stop();
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Disconnected\r\n");
        memset((uint8_t *)&m_blesc_config, 0, sizeof(configuration_t));
        advertising_start();
        break;

    case CONFIG_S_SERVER_EVT_PUBLISH:
        bleam_inactivity_timer_stop();
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Published\r\n");
        if (m_chunks_cnt < 1 || m_chunks_cnt >= BLESC_PUBLIC_KEY_SIZE / APP_CONFIG_DATA_CHUNK_SIZE) {
            // All chunks sent
            m_chunks_cnt = 0;
            bleam_inactivity_timer_start();
            break;
        }
        err_code = config_s_publish_pubkey_chunk(p_config_s_server, m_chunks_cnt + 1, m_blesc_public_key + (APP_CONFIG_DATA_CHUNK_SIZE * m_chunks_cnt));
        if (err_code != NRF_ERROR_INVALID_STATE) {
            APP_ERROR_CHECK(err_code);
        }
        ++m_chunks_cnt;
        bleam_inactivity_timer_start();
        break;

    case CONFIG_S_SERVER_EVT_WRITE: {
        bleam_inactivity_timer_stop();

        ble_gatts_evt_write_t const *p_evt_write = p_evt->write_evt_params;

        /** BLEAM_PUBKEY characteristic */
        if (p_evt_write->handle == p_config_s_server->char_handles[CONFIG_S_BLEAM_PUBKEY].value_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Write Bleam public key\r\n");
            config_service_on_bleam_pubkey(p_config_s_server, p_evt_write);
        } else
        /** NODE ID characteristic */
        if (p_evt_write->handle == p_config_s_server->char_handles[CONFIG_S_NODE_ID].value_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Write node ID\r\n");
            config_service_on_node_id(p_config_s_server, p_evt_write);
        } else
        /** STATUS characteristic */
        if (p_evt_write->handle == p_config_s_server->char_handles[CONFIG_S_STATUS].value_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Write status\r\n");
            config_service_on_recv_status(p_config_s_server, p_evt_write);
        } else
        /** Subscription to STATUS characteristic */
        if (p_evt_write->handle == p_config_s_server->char_handles[CONFIG_S_STATUS].cccd_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Subscription to status\r\n");
            config_s_status_update(p_config_s_server, config_s_get_status());
        } else 
        /** Subscription to BLESC_PUBKEY characteristic */
        if (p_evt_write->handle == p_config_s_server->char_handles[CONFIG_S_BLESC_PUBKEY].cccd_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Subscription to Bleam Scanner public key\r\n");
            config_service_on_sub_blesc_pubkey(p_config_s_server);
        } else {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config Service: Write event unhandled\r\n");
            // default
        }

        bleam_inactivity_timer_start();
        break;
    }

    case CONFIG_S_SERVER_EVT_FAIL: {
        // Fail but not fail? Cannot happen
        ASSERT(CONFIG_S_NO_FAIL != m_blesc_status_fail_detailed);
        if (CONFIG_S_FAIL_FDS == m_blesc_status_fail_detailed) {
            // FDS error: reinit FDS and reset
#if defined(SDK_15_3)
            nrf_sdh_disable_request();
#endif
#if defined(SDK_12_3)
            softdevice_handler_sd_disable();
#endif
            flash_pages_erase();
            sd_nvic_SystemReset();
        } else {
            // Bad data, BLEAM-size fail or timeout: just disconnect
            err_code = sd_ble_gap_disconnect(p_config_s_server->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (NRF_ERROR_INVALID_STATE != err_code)
                APP_ERROR_CHECK(err_code);
        }
        break;
    }

    default:
        // No implementation needed.
        break;
    }
}

void config_mode_services_init(config_s_server_t * p_config_s_server) {
    uint32_t err_code;
#if defined(HARDCODED_CONFIG)
    configuration_t hardconfig = {
        .node_id = IOS_TESTING_BLESC_NODE_ID,
        .keys = {
            .blesc_private_key = IOS_TESTING_BLESC_PRIVATE,
            .bleam_public_key  = IOS_TESTING_BLEAM_PUBLIC,
        }
    };
    memcpy(&m_blesc_config, &hardconfig, sizeof(configuration_t));
    err_code = flash_config_write();
    APP_ERROR_CHECK(err_code);
#else // !defined(HARDCODED_CONFIG)
    config_s_server_init_t config_s_init = {0};

    // Initialise Bleam Scanner configuration service
    config_s_init.evt_handler = config_s_event_handler;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&config_s_init.char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&config_s_init.char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&config_s_init.char_attr_md.write_perm);

    m_config_s_server = p_config_s_server;

    err_code = config_s_server_init(m_config_s_server, &config_s_init);
    APP_ERROR_CHECK(err_code);
#endif
}

uint16_t blesc_node_id_get(void) {
    return m_blesc_config.node_id;
}

blesc_params_t * blesc_params_get(void) {
    return &m_blesc_params;
}

blesc_keys_t * blesc_keys_get(void) {
    return &m_blesc_config.keys;
}

void config_status_update(config_s_status_t p_status) {
#ifdef HARDCODED_CONFIG
    if(CONFIG_S_STATUS_SET == p_status)
        leave_config_mode();
    else
        __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "Failed to config/r/n");
#else // !defined(HARDCODED_CONFIG)
    config_s_status_update(m_config_s_server, p_status);
#endif
}

/** @}*/