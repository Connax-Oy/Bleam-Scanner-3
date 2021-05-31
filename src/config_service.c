/** @file config_service.c
 *
 * @defgroup config_service Configuration service server
 * @{
 * @ingroup blesc_tasks
 * @ingroup blesc_config
 *
 * @brief BLE service for Bleam Scanner node configuration
 */
#include "config_service.h"
#include "log.h"
#include "sdk_common.h"
#include "app_error.h"

config_s_status_t m_config_s_status = CONFIG_S_STATUS_FAIL;  /**< Configuration process status */

/*************************** Config service handlers ****************************/

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_config_s_server        Pointer to the struct of Configuration service.
 * @param[in]   p_ble_evt                Pointer to event received from the BLE stack.
 *
 * @returns Nothing.
 */
static void on_connect(config_s_server_t *p_config_s_server, ble_evt_t const *p_ble_evt) {
    if ((NULL == p_config_s_server) || (NULL == p_ble_evt)) {
        return;
    }
    if(CONFIG_S_STATUS_DONE == config_s_get_status())
        return;

    p_config_s_server->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

    config_s_server_evt_t evt;

    evt.evt_type = CONFIG_S_SERVER_EVT_CONNECTED;

    p_config_s_server->evt_handler(p_config_s_server, &evt);
}

/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_config_s_server        Pointer to the struct of Configuration service.
 * @param[in]   p_ble_evt                Pointer to event received from the BLE stack.
 *
 * @returns Nothing.
 */
static void on_disconnect(config_s_server_t *p_config_s_server, ble_evt_t const *p_ble_evt) {
    if (NULL == p_config_s_server) {
        return;
    }
    UNUSED_PARAMETER(p_ble_evt);
    p_config_s_server->conn_handle = BLE_CONN_HANDLE_INVALID;
    m_config_s_status = CONFIG_S_STATUS_WAITING;

    config_s_server_evt_t evt;

    evt.evt_type = CONFIG_S_SERVER_EVT_DISCONNECTED;

    p_config_s_server->evt_handler(p_config_s_server, &evt);
}

/**@brief Function for handling the Write event.
 *
 * @param[in]   p_config_s_server        Pointer to the struct of Configuration service.
 * @param[in]   p_ble_evt                Event received from the BLE stack.
 *
 * @returns Nothing.
 */
static void on_write(config_s_server_t *p_config_s_server, ble_evt_t const *p_ble_evt) {
    if (NULL == p_config_s_server) {
        return;
    }
    config_s_server_evt_t evt = {
        .evt_type = CONFIG_S_SERVER_EVT_WRITE,
        .write_evt_params = &p_ble_evt->evt.gatts_evt.params.write
    };

    p_config_s_server->evt_handler(p_config_s_server, &evt);
}

/**@brief Function for handling the Publish event.
 *
 * @param[in]   p_config_s_server        Pointer to the struct of Configuration service.
 * @param[in]   p_ble_evt                Event received from the BLE stack.
 *
 * @returns Nothing.
 */
static void on_publish(config_s_server_t *p_config_s_server, ble_evt_t const *p_ble_evt) {
    if (NULL == p_config_s_server) {
        return;
    }
    config_s_server_evt_t evt = {
        .evt_type = CONFIG_S_SERVER_EVT_PUBLISH
    };

    p_config_s_server->evt_handler(p_config_s_server, &evt);
}

/**@brief Function for handling the Fail event.
 *
 * @param[in]   p_config_s_server        Pointer to the struct of Configuration service.
 * @param[in]   p_ble_evt                Event received from the BLE stack.
 *
 * @returns Nothing.
 */
static void on_fail(config_s_server_t *p_config_s_server, ble_evt_t const *p_ble_evt) {
    if (NULL == p_config_s_server) {
        return;
    }
    config_s_server_evt_t evt = {
        .evt_type = CONFIG_S_SERVER_EVT_FAIL
    };

    p_config_s_server->evt_handler(p_config_s_server, &evt);
}

void config_s_server_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context) {
    if ((NULL == p_context) || (NULL == p_ble_evt) || CONFIG_S_STATUS_DONE == config_s_get_status()) {
        return;
    }
    config_s_server_t *p_config_s_server = (config_s_server_t *)p_context;

    switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        on_connect(p_config_s_server, p_ble_evt);
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        on_disconnect(p_config_s_server, p_ble_evt);
        break;

    case BLE_GATTS_EVT_WRITE:
        on_write(p_config_s_server, p_ble_evt);
        break;
#if defined(SDK_15_3)
    case BLE_GATTS_EVT_HVN_TX_COMPLETE:
#endif
#if defined(SDK_12_3)
    case BLE_EVT_TX_COMPLETE:
#endif
        if (CONFIG_S_STATUS_FAIL == config_s_get_status())
            on_fail(p_config_s_server, p_ble_evt);
        else if (CONFIG_S_STATUS_WAITING == config_s_get_status())
            on_publish(p_config_s_server, p_ble_evt);
        break;
    default:
        break;
    }
}

/*************************** Config service init ****************************/

/**@brief Function for adding a Configuration Service characteristic.
 *
 * @param[in]       p_config_s_server        Configuration Service structure.
 * @param[in]       p_config_s_server_init   Information needed to initialize the service.
 * @param[in,out]   char_id                  Id for the new characteristic.
 * @param[in]       char_prop_read           Flag denoting if the characteristic had a Read mode.
 * @param[in]       char_prop_write          Flag denoting if the characteristic had a Write mode.
 * @param[in]       char_prop_notify         Flag denoting if the characteristic had a Notify mode.
 * @param[in]       char_prop_max_len        Maximum length of data that can pass through this characteristic.
 *
 * @retval NRF_SUCCESS on successful characteristic addition.
 * @retval NRF_ERROR_NULL if any of the provided pointers are NULL.
 * @returns otherwise, an error code from SDK 15.3.0 @link_sd_ble_gatts_characteristic_add or SDK 12.3.0 @link_12_sd_ble_gatts_characteristic_add.
 */
static uint32_t config_s_char_add(config_s_server_t *p_config_s_server,
                                config_s_server_init_t *p_config_s_server_init,
                                uint16_t char_id,
                                bool char_prop_read,
                                bool char_prop_write, 
                                bool char_prop_notify, 
                                uint16_t char_prop_max_len) {
    if ((NULL == p_config_s_server) || (NULL == p_config_s_server_init)) {
        return NRF_ERROR_NULL;
    }
    uint32_t err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t attr_char_value;
    ble_uuid_t ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));

    //  Read  operation on cccd should be possible without authentication.
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    
    cccd_md.write_perm = p_config_s_server_init->char_attr_md.cccd_write_perm;
    cccd_md.vloc       = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = char_prop_read;   // Read mode
    char_md.char_props.write  = char_prop_write;  // Write mode
    char_md.char_props.notify = char_prop_notify; // Notify mode
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;

    ble_uuid.type = p_config_s_server->uuid_type;
    ble_uuid.uuid = CONFIG_S_UUID | char_id; // Characteristic UUID

    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_config_s_server_init->char_attr_md.read_perm;
    attr_md.write_perm = p_config_s_server_init->char_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = char_prop_max_len;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = char_prop_max_len; // Max length of data

    err_code = sd_ble_gatts_characteristic_add(p_config_s_server->service_handle, &char_md, &attr_char_value, &p_config_s_server->char_handles[char_id]);
//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Added char 0x%04X, READ %d, WRITE %d, data len %d\r\n", CONFIG_S_UUID | char_id, char_prop_read, char_prop_write, char_prop_max_len);
    return err_code;
}

/*************************** Config service interface ****************************/

uint32_t config_s_server_init(config_s_server_t *p_config_s_server, config_s_server_init_t *p_config_s_server_init) {
    if ((NULL == p_config_s_server) || (NULL == p_config_s_server_init)) {
        return NRF_ERROR_NULL;
    }
    uint32_t err_code;

    ble_uuid128_t config_s_base_uuid = {CONFIG_S_BASE_UUID};
#if defined(SDK_15_3)
    err_code = nrf_crypto_rng_vector_generate((uint8_t *)&(config_s_base_uuid.uuid128[2]), 10);
#endif
#if defined(SDK_12_3)
    err_code = nrf_drv_rng_rand((uint8_t *)&(config_s_base_uuid.uuid128[2]), 10);
#endif
    APP_ERROR_CHECK(err_code);

    ble_uuid_t config_s_uuid;

    if (NULL == p_config_s_server || NULL == p_config_s_server_init || NULL == p_config_s_server_init->evt_handler)
        return NRF_ERROR_NULL;
    p_config_s_server->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_config_s_server->evt_handler = p_config_s_server_init->evt_handler;
    err_code = sd_ble_uuid_vs_add(&config_s_base_uuid, &p_config_s_server->uuid_type);
    if (NRF_SUCCESS != err_code) {
        return err_code;
    }
    config_s_uuid.type = p_config_s_server->uuid_type;
    config_s_uuid.uuid = CONFIG_S_UUID;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &config_s_uuid, &p_config_s_server->service_handle);
    if (NRF_SUCCESS != err_code) {
        return err_code;
    }

    // Add characteristics
    err_code = err_code | config_s_char_add(p_config_s_server, p_config_s_server_init, CONFIG_S_VERSION,      1, 0, 0, 4);
    err_code = err_code | config_s_char_add(p_config_s_server, p_config_s_server_init, CONFIG_S_STATUS,       1, 1, 1, 1);
    err_code = err_code | config_s_char_add(p_config_s_server, p_config_s_server_init, CONFIG_S_BLESC_PUBKEY, 1, 0, 1, 17);
    err_code = err_code | config_s_char_add(p_config_s_server, p_config_s_server_init, CONFIG_S_BLEAM_PUBKEY, 0, 1, 0, 17);
    err_code = err_code | config_s_char_add(p_config_s_server, p_config_s_server_init, CONFIG_S_NODE_ID,      0, 1, 0, 2);

    if(NRF_SUCCESS == err_code) {
        m_config_s_status = CONFIG_S_STATUS_WAITING;
    }

    return err_code;
}

uint32_t config_s_publish_version(config_s_server_t *p_config_s_server) {
    if (p_config_s_server == NULL)
        return NRF_ERROR_NULL;
    if (p_config_s_server->conn_handle == BLE_CONN_HANDLE_INVALID) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Connection handle invalid.\r\n");
        return NRF_ERROR_INVALID_STATE;
    }

    config_s_server_version_t version_data = {
        .protocol_id = APP_CONFIG_PROTOCOL_NUMBER,
        .fw_id       = APP_CONFIG_FW_VERSION_ID,
        .hw_id       = HW_ID,
    };

    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len = sizeof(config_s_server_version_t);
    gatts_value.offset = 0;
    gatts_value.p_value = (uint8_t *)(&version_data);

    // Update database.
    err_code = sd_ble_gatts_value_set(p_config_s_server->conn_handle,
                                      p_config_s_server->char_handles[CONFIG_S_VERSION].value_handle,
                                      &gatts_value);
    if (NRF_SUCCESS != err_code)
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Gatts value set result: %04X\r\n", err_code);
    return err_code;
}

uint32_t config_s_status_update(config_s_server_t *p_config_s_server, config_s_status_t p_status) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Update status to %d on handle 0x%04X\r\n", p_status, CONFIG_S_UUID | CONFIG_S_STATUS);
    if (p_config_s_server == NULL)
        return NRF_ERROR_NULL;
    if (p_config_s_server->conn_handle == BLE_CONN_HANDLE_INVALID) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Connection handle invalid.\r\n");
        return NRF_ERROR_INVALID_STATE;
    }

    m_config_s_status = p_status;

    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len = 1;
    gatts_value.offset = 0;
    gatts_value.p_value = (uint8_t *)(&p_status);
    
    // Update database.
    err_code = sd_ble_gatts_value_set(p_config_s_server->conn_handle,
                                      p_config_s_server->char_handles[CONFIG_S_STATUS].value_handle,
                                      &gatts_value);
    if (NRF_SUCCESS != err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Gatts value set result: %04X\r\n", err_code);
    }

    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_config_s_server->char_handles[CONFIG_S_STATUS].value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = gatts_value.offset;
    hvx_params.p_len  = &gatts_value.len;
    hvx_params.p_data = gatts_value.p_value;

    err_code = sd_ble_gatts_hvx(p_config_s_server->conn_handle, &hvx_params);
    if (NRF_SUCCESS != err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "sd_ble_gatts_hvx result: %04X\r\n", err_code);
        if (BLE_ERROR_GATTS_SYS_ATTR_MISSING == err_code)
            err_code = NRF_SUCCESS;
    }
    return err_code;
}

uint32_t config_s_publish_pubkey_chunk(config_s_server_t *p_config_s_server, uint8_t chunk_number, uint8_t * pubkey_chunk) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Publish Bleam Scanner public key chunk %u on handle 0x%04X\r\n", chunk_number, CONFIG_S_UUID | CONFIG_S_BLESC_PUBKEY);
    if (p_config_s_server == NULL || pubkey_chunk == NULL)
        return NRF_ERROR_NULL;
    if (p_config_s_server->conn_handle == BLE_CONN_HANDLE_INVALID) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Connection handle invalid.\r\n");
        return NRF_ERROR_INVALID_STATE;
    }
    if (chunk_number > 4)
        return NRF_ERROR_INVALID_PARAM;

    uint8_t data_array[17];
    data_array[0] = chunk_number;
    memcpy(data_array + 1, pubkey_chunk, 16);

    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len = 17;
    gatts_value.offset = 0;
    gatts_value.p_value = data_array;
    
    // Update database.
    err_code = sd_ble_gatts_value_set(p_config_s_server->conn_handle,
                                      p_config_s_server->char_handles[CONFIG_S_BLESC_PUBKEY].value_handle,
                                      &gatts_value);
    if (NRF_SUCCESS != err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Gatts value set result: %04X\r\n", err_code);
        return err_code;
    }

    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_config_s_server->char_handles[CONFIG_S_BLESC_PUBKEY].value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = gatts_value.offset;
    hvx_params.p_len  = &gatts_value.len;
    hvx_params.p_data = gatts_value.p_value;

    err_code = sd_ble_gatts_hvx(p_config_s_server->conn_handle, &hvx_params);
    if (NRF_SUCCESS != err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "sd_ble_gatts_hvx result: %04X\r\n", err_code);
        if (BLE_ERROR_GATTS_SYS_ATTR_MISSING == err_code)
            err_code = NRF_SUCCESS;
    }
    return err_code;
}

config_s_status_t config_s_get_status(void) {
    return m_config_s_status;
}

void config_s_finish(void) {
    m_config_s_status = CONFIG_S_STATUS_DONE;
}

/** @}*/