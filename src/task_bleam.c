/** @file task_bleam.c
 *
 * @defgroup task_bleam Task Bleam
 * @{
 * @ingroup bleam_connect
 * @ingroup blesc_tasks
 *
 * @brief Handles Bleam events of connection and data sharing on high level.
 */

#include "task_bleam.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#include "task_board.h"
#include "task_config.h"
#include "task_scan_connect.h"
#include "task_connect_common.h"
#include "task_time.h"

static __ALIGN(4) uint8_t              m_bleam_signature[BLESC_SIGNATURE_SIZE]; /**< Signature received from Bleam */
static uint8_t                         m_blesc_salt[SALT_SIZE];                 /**< Salt sent to Bleam */
static uint8_t                         m_blesc_request_data[SALT_SIZE];         /**< Data from Bleam Scanner request */
static bleam_service_client_cmd_type_t m_blesc_cmd;                             /**< Type of action command Bleam Scanner received from Bleam Tools */
extern blesc_params_t                  m_blesc_params;                          /**< Bleam Scanner params, extern from task_fds.h */

#ifdef BLESC_DFU
ret_code_t enter_dfu_mode(void); // forward declaration
#endif

/********** Helper functions ***********/

void bleam_connection_abort(bleam_service_client_t *p_bleam_client) {
    bleam_service_mode_set(BLEAM_SERVICE_CLIENT_MODE_NONE);
    m_blesc_cmd = NULL;
    ret_code_t err_code = sd_ble_gap_disconnect(p_bleam_client->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (NRF_ERROR_INVALID_STATE != err_code)
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for extracting Bleam device type from its 128-bit UUID.
 *
 * @param[in] data       Pointer to Bleam device data record.
 *
 * @returns @ref bleam_service_type_t Bleam service type of the Bleam device.
 */
static bleam_service_type_t get_bleam_type(blesc_model_rssi_data_t * data) {
    ASSERT(NULL != data);
    return (bleam_service_type_t)(data->bleam_uuid[0]);
}

/************ Bleam service partial handlers *************/

/**@brief Handler of the beginning of intercations with Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 *
 * @returns Nothing.
 */
static void bleam_service_on_start(bleam_service_client_t *p_bleam_client) {
    ret_code_t err_code = bleam_service_client_notify_enable(p_bleam_client);
    if (NRF_SUCCESS != err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Can't enable notify, remote disconnected\r\n");
        bleam_connection_abort(p_bleam_client);
    }
    bleam_send_init(p_bleam_client);

    // IOS Bleam won't send salt if there is already a Bleam Scanner connection happening
    bleam_inactivity_timer_start();
}

/**@brief Handler for the event of receiving @ref BLEAM_SERVICE_CLIENT_CMD_SALT command from Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 *
 * @returns Nothing.
 */
static void bleam_service_on_bleam_salt(bleam_service_client_t *p_bleam_client,
                                 bleam_service_client_evt_t *p_evt) {
    blesc_model_rssi_data_t *bleam_device = get_connected_bleam_data();
    blesc_keys_t *keys = blesc_keys_get();

    bleam_service_mode_set(BLEAM_SERVICE_CLIENT_MODE_RSSI);
    uint8_t salt[SALT_SIZE];
    uint8_t digest[BLESC_SIGNATURE_SIZE];
    memcpy(salt, p_evt->p_data + 2, SALT_SIZE);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Received salt", salt, SALT_SIZE);
    sign_data(digest, salt, keys);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Signature", digest, BLESC_SIGNATURE_SIZE);
    bleam_send_signature(digest, BLESC_SIGNATURE_SIZE);
}

/**@brief Handler for the event of receiving @ref BLEAM_SERVICE_CLIENT_CMD_TRUST command.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 *
 * @returns Nothing.
 */
static void bleam_service_on_bleam_trust(bleam_service_client_t *p_bleam_client) {
    bleam_service_mode_set(BLEAM_SERVICE_CLIENT_MODE_RSSI);
    
    // Emulate done sending signature event.
    bleam_service_client_evt_t evt;
    evt.evt_type = BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_SIGNATURE;
    p_bleam_client->evt_handler(p_bleam_client, &evt);
}

/**@brief Handler for the event of receiving @ref BLEAM_SERVICE_CLIENT_CMD_SIGN command from Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 *
 * @returns Nothing.
 */
static void bleam_service_on_bleam_signature_chunk(bleam_service_client_t *p_bleam_client,
                                           bleam_service_client_evt_t *p_evt) {
    blesc_model_rssi_data_t *bleam_device = get_connected_bleam_data();
    blesc_keys_t *keys = blesc_keys_get();

    ret_code_t err_code = NRF_SUCCESS;

    uint8_t chunk_number = p_evt->p_data[1];
    // Check if chunk number is valid
    if (0 == chunk_number || BLESC_SIGNATURE_SIZE / APP_CONFIG_DATA_CHUNK_SIZE < chunk_number) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received invalid signature chunk number.\r\n");
        bleam_connection_abort(p_bleam_client);
        return;
    }
    recvd_chunks_add(chunk_number - 1);
    memcpy(m_bleam_signature + (APP_CONFIG_DATA_CHUNK_SIZE * (chunk_number - 1)), p_evt->p_data + 2, APP_CONFIG_DATA_CHUNK_SIZE);

    // If all chunks have been received
    if (recvd_chunks_validate(BLESC_SIGNATURE_SIZE / APP_CONFIG_DATA_CHUNK_SIZE)) {
        // If signature received is incorrect, disconnect
        if (!sign_verify(m_bleam_signature, m_blesc_salt, keys)) {
            add_raw_in_blacklist(bleam_device->raw);
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam signature failed verification.\r\n");
            bleam_connection_abort(p_bleam_client);
            return;
        }
        // If signature matches, do da thing
        if (BLEAM_SERVICE_CLIENT_CMD_DFU == m_blesc_cmd) {
#if defined(BLESC_DFU)
            // Enter DFU
            enter_dfu_mode();
#else // !defined(BLESC_DFU)
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "This firmware does not support DFU.\r\n");
            bleam_connection_abort(p_bleam_client);
#endif
        } else if (BLEAM_SERVICE_CLIENT_CMD_REBOOT == m_blesc_cmd) {
            // Reboot board
            sd_nvic_SystemReset();
        } else if (BLEAM_SERVICE_CLIENT_CMD_UNCONFIG == m_blesc_cmd) {
            // Delete config data
            flash_config_delete();
        } else if (BLEAM_SERVICE_CLIENT_CMD_IDLE == m_blesc_cmd) {
            // Switch to IDLE and set wakeup time
            eco_timer_handler(NULL);
            uint32_t idle_time_minutes = (((uint32_t)m_blesc_request_data[0]) << 1) | (uint32_t)m_blesc_request_data[1];
            blesc_set_idle_time_minutes(idle_time_minutes);
        } else if (BLEAM_SERVICE_CLIENT_CMD_RSSI_LIMIT == m_blesc_cmd) {
            // Set new lower RSSI level limit
            m_blesc_params.rssi_lower_limit = (int8_t)m_blesc_request_data[0];
            flash_params_update();            
        } else {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Wrong Bleam Scanner mode to receive signature.\r\n");
            bleam_connection_abort(p_bleam_client);            
        }
        m_blesc_cmd = NULL;
        memset(&m_blesc_request_data, 0, SALT_SIZE);
    } else {
        // Wait for the next signature chunk
        bleam_inactivity_timer_start();
    }
}

/**@brief Handler for the event of receiving an action command from Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 * @param[in] cmd                  Action command from Bleam.
 *
 * @returns Nothing.
 */
static void bleam_service_on_bleam_request(bleam_service_client_t *p_bleam_client,
                                    bleam_service_client_evt_t *p_evt,
                                    uint8_t cmd) {
    blesc_model_rssi_data_t *bleam_device = get_connected_bleam_data();
    ASSERT(NULL != bleam_device)

    ret_code_t err_code = NRF_SUCCESS;
    bleam_service_mode_set(BLEAM_SERVICE_CLIENT_MODE_CMD);
    m_blesc_cmd = cmd;

    // Prepare for DFU mode
    if (BLEAM_SERVICE_CLIENT_CMD_DFU == cmd) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received request to enter DFU mode.\r\n");
    } else 
    // Prepare for node reboot
    if (BLEAM_SERVICE_CLIENT_CMD_REBOOT == cmd) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received request to reboot.\r\n");
    } else
    // Prepare for node unconfiguration
    if (BLEAM_SERVICE_CLIENT_CMD_UNCONFIG == cmd) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received request to unconfigure.\r\n");
    } else
    // Prepare for idling for N minutes
    if (BLEAM_SERVICE_CLIENT_CMD_IDLE == cmd) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received request to idle for %u minutes.\r\n", (((uint32_t)p_evt->p_data[2]) << 1) | (uint32_t)p_evt->p_data[3]);
        m_blesc_request_data[0] = p_evt->p_data[2];
        m_blesc_request_data[1] = p_evt->p_data[3];
    } else
    // Prepare for setting new lower RSSI limit
    if (BLEAM_SERVICE_CLIENT_CMD_RSSI_LIMIT == cmd) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received request to set lower RSSI limit to %d.\r\n", (int8_t)p_evt->p_data[2]);
        m_blesc_request_data[0] = p_evt->p_data[2];
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Impossible NOTIFY command %u\r\n", cmd);
        bleam_connection_abort(p_bleam_client);
        return;
    }

    // Send salt to Bleam to confirm Bleam is genuine
    memset(m_blesc_salt, 0, SALT_SIZE);
#if defined(SDK_15_3)
    err_code = nrf_crypto_rng_vector_generate(m_blesc_salt, SALT_SIZE);
#endif
#if defined(SDK_12_3)
    err_code = nrf_drv_rng_rand(m_blesc_salt, SALT_SIZE);
#endif
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Generated salt", m_blesc_salt, SALT_SIZE);
    memset(m_bleam_signature, 0, BLESC_SIGNATURE_SIZE);
    bleam_send_signature(m_blesc_salt, SALT_SIZE);
}

/**@brief Handler for the event of finishing sending RSSI data to Bleam.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 * @param[in] bleam_device         Pointer to the Bleam device data record.
 *
 * @returns Nothing.
 */
static void bleam_service_on_done_sending(bleam_service_client_t *p_bleam_client,
                                          bleam_service_client_evt_t *p_evt,
                                          blesc_model_rssi_data_t *bleam_device) {
    raw_in_whitelist(bleam_device->raw);
    clear_rssi_data(bleam_device);
}

/**@brief Handler for the event of receiving time data from Bleam.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 *
 * @returns Nothing.
 */
static void bleam_service_on_time(bleam_service_client_t *p_bleam_client,
                                  bleam_service_client_evt_t *p_evt) {
    ASSERT(NULL != p_evt->p_data);
    ASSERT(sizeof(uint32_t) != p_evt->data_len);
    uint32_t new_time;
    memcpy(&new_time, p_evt->p_data, sizeof(uint32_t));
    if (system_time_needs_update_get()) {
        system_time_update(new_time);
    }
}

/**@brief Handler of disconnection from Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 * @param[in] bleam_device         Pointer to the Bleam device data record.
 *
 * @returns Nothing.
 */
static void bleam_service_on_disconnect(bleam_service_client_t *p_bleam_client,
                                        bleam_service_client_evt_t *p_evt,
                                        blesc_model_rssi_data_t *bleam_device) {
    // clear data just in case
    clear_rssi_data(bleam_device);
    bleam_service_mode_set(BLEAM_SERVICE_CLIENT_MODE_NONE);
    bleam_send_uninit();
}

/**@brief Handler of the lack of Bleam service on the supposed Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 * @param[in] bleam_device         Pointer to the Bleam device data record.
 *
 * @returns Nothing.
 */
static void bleam_service_on_srv_not_found(bleam_service_client_t *p_bleam_client,
                                           bleam_service_client_evt_t *p_evt,
                                           blesc_model_rssi_data_t *bleam_device) {
    if (!raw_in_blacklist(bleam_device->raw))
        add_raw_in_blacklist(bleam_device->raw);
    clear_rssi_data(bleam_device);
}

/**@brief Handler of the botched connection to a Bleam device.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 * @param[in] p_evt                Pointer to the event data.
 * @param[in] bleam_device         Pointer to the Bleam device data record.
 *
 * @returns Nothing.
 */
static void bleam_service_on_bad_connection(bleam_service_client_t *p_bleam_client,
                                            bleam_service_client_evt_t *p_evt,
                                            blesc_model_rssi_data_t *bleam_device) {
    clear_rssi_data(bleam_device);
}

void bleam_service_evt_handler(bleam_service_client_t *p_bleam_client, bleam_service_client_evt_t *p_evt) {
    ret_code_t err_code;
    switch (p_evt->evt_type) {
    case BLEAM_SERVICE_CLIENT_EVT_DISCOVERY_COMPLETE: {
        bleam_inactivity_timer_stop();
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Service discovery complete\r\n");
        recvd_chunks_clear();
        m_blesc_cmd = NULL;

        blesc_model_rssi_data_t * bleam_device = get_connected_bleam_data();
        if(BLEAM_SERVICE_TYPE_IOS == get_bleam_type(bleam_device)) {
            // Send MAC and Node ID first
            bleam_service_mac_info_t mac_info = {
                .node_id = blesc_node_id_get(),
            };
            ble_gap_addr_t mac;
#if defined(SDK_15_3)
            sd_ble_gap_addr_get(&mac);
#endif
#if defined(SDK_12_3)
            sd_ble_gap_address_get(&mac);
#endif
            memcpy(mac_info.mac, mac.addr, BLE_GAP_ADDR_LEN);
            err_code = bleam_service_data_send(p_bleam_client, (uint8_t *)(&mac_info), sizeof(bleam_service_mac_info_t), BLEAM_S_MAC);
            if (NRF_ERROR_INVALID_STATE != err_code) {
                // If the MAC char isn't present on iOS BLEAM,
                // treat it like it doesn't have Bleam service at all
                if (NRF_ERROR_NOT_FOUND == err_code) {
                    bleam_service_on_srv_not_found(p_bleam_client, p_evt, bleam_device);
                    err_code = sd_ble_gap_disconnect(p_bleam_client->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                    if (NRF_ERROR_INVALID_STATE != err_code)
                        APP_ERROR_CHECK(err_code);
                    return;
                } else {
                    APP_ERROR_CHECK(err_code);
                }
            } // else the BLE DISCONNECTED event will be called            
            return;
        }
        
        bleam_service_on_start(p_bleam_client);
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_RECV_SALT: {
        bleam_inactivity_timer_stop();

        uint8_t cmd = p_evt->p_data[0];
        // Salt for regular Bleam connect
        if (BLEAM_SERVICE_CLIENT_CMD_SALT == cmd) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Received salt\r\n");
            bleam_service_on_bleam_salt(p_bleam_client, p_evt);
        } else
        // Skip salt and signature, send HEALTH and RSSI data
        if (BLEAM_SERVICE_CLIENT_CMD_TRUST == cmd) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Received trust\r\n");
            bleam_service_on_bleam_trust(p_bleam_client);
        } else
        // Part of Bleam signature
        if (BLEAM_SERVICE_CLIENT_CMD_SIGN == cmd) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Received sign chunk\r\n");
            bleam_service_on_bleam_signature_chunk(p_bleam_client, p_evt);
        } else {
        // Command for other interaction protocol
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Received NOTIFY command %u\r\n", cmd);
            bleam_service_on_bleam_request(p_bleam_client, p_evt, cmd);
            // Wait for signature in salt
            bleam_inactivity_timer_start();
        }
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_PUBLISH: {
        if (BLEAM_SERVICE_CLIENT_MODE_NONE == bleam_service_mode_get()) {
            bleam_service_on_start(p_bleam_client);
        } else {
            bleam_send_continue();
        }
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_SIGNATURE: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Done sending signature\r\n");

        if(BLEAM_SERVICE_CLIENT_MODE_RSSI == bleam_service_mode_get()) {
            // Collect and send health data
            battery_level_measure();
        }
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_HEALTH: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Done sending health\r\n");

        if(BLEAM_SERVICE_CLIENT_MODE_RSSI == bleam_service_mode_get()) {
            blesc_model_rssi_data_t * bleam_device = get_connected_bleam_data();
            // Collect and send RSSI data
            for(uint8_t cnt = 0; APP_CONFIG_RSSI_PER_MSG > cnt; ++cnt) {
                bleam_rssi_queue_add(bleam_device->rssi[cnt], bleam_device->aoa[cnt]);
            }
            bleam_send_continue();
        }
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_RSSI: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Done sending data\r\n");
        bleam_service_on_done_sending(p_bleam_client, p_evt, get_connected_bleam_data());

        // Wind up the clock
        if (system_time_needs_update_get()) {
            if (NRF_SUCCESS == bleam_service_client_read_time(p_bleam_client)) {
                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Requesting time from BLEAM\r\n");
                return;
            }
        }

        err_code = sd_ble_gap_disconnect(p_bleam_client->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if (NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_RECV_TIME: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Received time\r\n");
        bleam_service_on_time(p_bleam_client, p_evt);
        err_code = sd_ble_gap_disconnect(p_bleam_client->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if(NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_DISCONNECTED: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Disconnected\r\n");
        bleam_service_on_disconnect(p_bleam_client, p_evt, get_connected_bleam_data());
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_SRV_NOT_FOUND: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Bleam service not found\r\n");
        bleam_service_on_srv_not_found(p_bleam_client, p_evt, get_connected_bleam_data());
        stupid_ios_data_clear();
        err_code = sd_ble_gap_disconnect(p_bleam_client->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if(NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
        break;
    }

    case BLEAM_SERVICE_CLIENT_EVT_BAD_CONNECTION: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service event: Bad connection\r\n");
        bleam_service_on_bad_connection(p_bleam_client, p_evt, get_connected_bleam_data());
        err_code = sd_ble_gap_disconnect(p_bleam_client->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if(NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
        break;
    }

    default:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam service unhandled event\r\n");
    }
}

/** @}*/