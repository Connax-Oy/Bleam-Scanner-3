/** @file bleam_discovery.c
 *
 * @defgroup bleam_discovery Custom service discovery handler
 * @{
 * @ingroup blesc_tasks
 * @ingroup ios_solution
 *
 * @brief Custom service discovery handler
 */

#include "bleam_discovery.h"
#include "log.h"
#include "sdk_common.h"
#include "app_error.h"

#define BLE_GATTC_HANDLE_START 0x0001 /**< Default start GATTC handle value */
#define BLE_GATTC_HANDLE_END   0xFFFF /**< Default end GATTC handle value */


static bleam_service_discovery_evt_handler_t m_evt_handler = NULL; /**< Pointer to the function that will handle custom service discovery events. */

static uint16_t                 service_uuid_to_find; /**< 12th and 13th octets of the service UUID being discovered. */
static uint8_t                  m_uuid_128[16];       /**< Temporary storage for found 128-bit service UUID. */
static ble_gattc_handle_range_t handle_range;         /**< Range of service handles to look the service within. */
static bool                     m_discovery_started;  /**< Flag denoting whether this service discovery is going. */
static bool                     service_found;        /**< Flag denoting whether the service was discovered successfully. */
static uint16_t                 gattc_read_pending;   /**< The amount of pending sd_ble_gattc_read requests. */

static uint16_t m_central_conn_handle = BLE_CONN_HANDLE_INVALID; /**< Connection handle. */

/**@brief Function for finishing custom service discovery.
 *
 * @returns Nothing.
 */
static void finish_discovery() {
    m_discovery_started = false;

    bleam_service_discovery_evt_t evt = {0};
    evt.conn_handle = m_central_conn_handle;

    if(service_found) {
        evt.params.err_code = NRF_SUCCESS;
        memcpy(evt.params.srv_uuid128.uuid128, m_uuid_128, 16);
        evt.params.srv_uuid16.uuid = service_uuid_to_find;
        evt.evt_type = BLEAM_SERVICE_DISCOVERY_COMPLETE;
        m_evt_handler(&evt);
    } else {
        if(!service_found) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Service %04X not found.\r\n", service_uuid_to_find);
        }
        evt.params.err_code = NRF_ERROR_NOT_FOUND;
        evt.evt_type = BLEAM_SERVICE_DISCOVERY_SRV_NOT_FOUND;
    }
    m_evt_handler(&evt);
}

/**@brief Function for handling error of this custom service discovery.
 *
 * @returns Nothing.
 */
static void error_discovery() {
    m_discovery_started = false;
    bleam_service_discovery_evt_t evt = {0};
    evt.params.err_code = NRF_ERROR_NOT_FOUND;
    evt.evt_type = BLEAM_SERVICE_DISCOVERY_ERROR;
    evt.conn_handle = m_central_conn_handle;
    m_evt_handler(&evt);
}

/**@brief Function for handling this custom service discovery.
 *
 * @returns Nothing.
 */
static void continue_discovery(void) {
    ret_code_t err_code;
    // Go to next handle.
    handle_range.start_handle++;

    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Scan continues.\r\n");

    if(!service_found) {
        // Discover next primary Service.
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Scanned handle range starts at 0x%04X.\r\n", handle_range.start_handle);
        err_code = sd_ble_gattc_primary_services_discover(m_central_conn_handle, handle_range.start_handle, NULL);
        if (NRF_ERROR_BUSY != err_code && NRF_SUCCESS != err_code) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "sd_ble_gattc_primary_services_discover returned error code 0x%04X\r\n", err_code);
            error_discovery();
        }
    } else {
        finish_discovery();
    }
}

/**@brief Function for handling primary service discovery event.
 *
 * @param[in] p_db_discovery        Pointer to the DB discovery context.
 * @param[in] p_ble_gattc_evt       Pointer to the GATTC event data.
 *
 * @returns Nothing.
 */
static void on_primary_srv_discovery_rsp(ble_db_discovery_t *p_db_discovery, ble_gattc_evt_t const *p_ble_gattc_evt) {
    uint32_t err_code = NRF_ERROR_NOT_FOUND;
    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Services found = %u\r\n", p_ble_gattc_evt->params.prim_srvc_disc_rsp.count);
    if (p_ble_gattc_evt->gatt_status != BLE_GATT_STATUS_SUCCESS) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "sd_ble_gattc_primary_services_discover returned gatt_status 0x%04X\r\n", p_ble_gattc_evt->gatt_status);
        if(0 == gattc_read_pending)
            finish_discovery();
        return;
    }
    // Go through all Services.
    for (uint16_t index = 0; index < p_ble_gattc_evt->params.prim_srvc_disc_rsp.count && !service_found; index++) {
        const ble_gattc_service_t * service = &p_ble_gattc_evt->params.prim_srvc_disc_rsp.services[index];
        // Log handle ranges for future use (in case this is our Service and we need to discover subsequent Characteristics).
        handle_range.start_handle = service->handle_range.start_handle;
        handle_range.end_handle = service->handle_range.end_handle;
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Service 0x%04X of type 0x%04X\r\n", service->uuid.uuid, service->uuid.type);
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Handle range: 0x%04X - 0x%04X\r\n", handle_range.start_handle, handle_range.end_handle);
        // We are interested only in one specific Service with 128-bit proprietary UUID
        // (assuming that we have only one such 128-bit UUID base registered in SoftDevice through "sd_ble_uuid_vs_add" call!).
        if (((service->uuid.uuid == service_uuid_to_find) && (service->uuid.type == BLE_UUID_TYPE_VENDOR_BEGIN)) ||
            (service->uuid.type == BLE_UUID_TYPE_UNKNOWN)) {
            // This may be the service we're looking for
            err_code = sd_ble_gattc_read(m_central_conn_handle, service->handle_range.start_handle, 0);
            if (NRF_SUCCESS != err_code) {
                __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "sd_ble_gattc_read returned error code 0x%04X\r\n", err_code);
                return;
            } else {
                ++gattc_read_pending;
            }
        } else {
            __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "Ignored Service index = %d, UUID = 0x%04X, UUID type = %d\r\n", index,
                service->uuid.uuid,
                service->uuid.type);
        }
    }
    // If My Service not found => jump to next handle range.
    if (!service_found) {
        handle_range.start_handle = handle_range.end_handle;
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Scanned handle range ends at 0x%04X.\r\n", handle_range.start_handle);
        if(0 != gattc_read_pending)
            return;
        if (handle_range.start_handle != BLE_GATTC_HANDLE_END) {
            continue_discovery();
        } else {
            // Service not found
            finish_discovery();
        }
    }
}

/**@brief Function for handling GATTC read response event.
 *
 * @param[in] p_ble_gattc_evt       Pointer to the GATTC event data.
 *
 * @returns Nothing.
 */
static void on_gattc_read_response(ble_gattc_evt_t const *p_ble_gattc_evt) {
    uint32_t err_code = NRF_ERROR_NOT_FOUND;        
    // Response should contain full 128-bit UUID.
    uint8_t *rsp_data = (uint8_t *)p_ble_gattc_evt->params.read_rsp.data;
    uint8_t rsp_data_len = p_ble_gattc_evt->params.read_rsp.len;
    if (!service_found && rsp_data_len == 16) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Custom Primary Service with 128-bit UUID found.\r\n");
        __LOG_XB(LOG_SRC_APP, LOG_LEVEL_DBG2, "128-bit UUID value\r\n", rsp_data, 16);
        // Check if it is Bleam service
        if (rsp_data[13] == ((service_uuid_to_find >> 8) & 0x00FF) && rsp_data[12] == (service_uuid_to_find & 0x00FF)) {
            // My Service found.
            service_found = true;
            __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "My Service found, UUID = 0x%04X, handle = 0x%04X\r\n",
                (uint16_t)(rsp_data[12] + 256 * rsp_data[13]),
                p_ble_gattc_evt->params.read_rsp.handle);
            // save UUID of this device
            memcpy(m_uuid_128, rsp_data, 16);
            finish_discovery();
            return;
        }
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Ignored Service, BLE_GATTC_EVT_READ_RSP len = %d\r\n", rsp_data_len);
    }
    --gattc_read_pending;
    if(gattc_read_pending == 0)
        continue_discovery();
}

void bleam_service_discovery_start(ble_db_discovery_t *const p_db_discovery, uint16_t conn_handle) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "INFO: Full Service Discovery (enumeration of stack on Peripheral/Server side over BLE GATT commands).\r\n");

    m_central_conn_handle = conn_handle;

    // Initiate discovery procedure ("tree-like" graph).
    handle_range.start_handle = BLE_GATTC_HANDLE_START;
    handle_range.end_handle = BLE_GATTC_HANDLE_END;
    m_discovery_started = true;
    service_found = false;
    gattc_read_pending   = 0;

    // Discover next primary Service.
    ret_code_t err_code = sd_ble_gattc_primary_services_discover(m_central_conn_handle, handle_range.start_handle, NULL);
    if (NRF_SUCCESS != err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "sd_ble_gattc_primary_services_discover returned error code 0x%04X\r\n", err_code);
    }
}

void bleam_service_discovery_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context) {
    VERIFY_PARAM_NOT_NULL_VOID(p_ble_evt);
    VERIFY_PARAM_NOT_NULL_VOID(p_context);
    if(!m_discovery_started)
        return;

    ble_db_discovery_t *p_db_discovery = (ble_db_discovery_t *)p_context;

    switch (p_ble_evt->header.evt_id) {
    case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
        on_primary_srv_discovery_rsp(p_db_discovery, &(p_ble_evt->evt.gattc_evt));
        break;
    case BLE_GATTC_EVT_READ_RSP:
        on_gattc_read_response(&(p_ble_evt->evt.gattc_evt));
        break;
    }
}

uint32_t bleam_service_discovery_init(bleam_service_discovery_evt_handler_t evt_handler, uint16_t p_uuid_to_find) {
    VERIFY_PARAM_NOT_NULL(evt_handler);
    m_evt_handler        = evt_handler;
    service_uuid_to_find = p_uuid_to_find;
    m_discovery_started  = false;

    return NRF_SUCCESS;
}

/** @}*/