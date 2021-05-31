#include <stdint.h>
#include <string.h>

/* HAL */
#include "boards.h"
#include "app_timer.h"

/* Core */
#include "nrf_mesh_configure.h"
#include "nrf_mesh.h"
#include "mesh_stack.h"
#include "device_state_manager.h"
#include "access_config.h"
#include "nrf_sdh_soc.h"

/* Provisioning and configuration */
#include "mesh_provisionee.h"
#include "mesh_app_utils.h"

/* Models */
#include "blesc_model.h"

/* Logging and RTT */
#include "nrf_log.h"

/* Example specific includes */
#include "app_blesc.h"
#include "app_config.h"
#include "nrf_mesh_config_examples.h"
#include "blesc_model_common.h"
#include "example_common.h"
#include "mesh_main.h"

#define APP_BLESC_ELEMENT_INDEX (0)
#define MESH_SOC_OBSERVER_PRIO   0

static bool                    m_device_provisioned; /**< Flag denoting if the device was provisioned or not */
mesh_main_config_complete_cb_t config_complete_cb;   /**< Callback for provisioning complete event for main. */

/**@brief Handler for SoC event */
static void mesh_soc_evt_handler(uint32_t evt_id, void * p_context)
{
    nrf_mesh_on_sd_evt(evt_id);
}

NRF_SDH_SOC_OBSERVER(m_mesh_soc_observer, MESH_SOC_OBSERVER_PRIO, mesh_soc_evt_handler, NULL);

/* BLESc model server structure definition and initialization */
APP_BLESC_MODEL_DEF(m_blesc_models,
                     APP_CONFIG_FORCE_SEGMENTATION,
                     APP_CONFIG_MIC_SIZE)
blesc_model_server_t * m_generic_server = &(m_blesc_models.server[BLESC_MODEL_GENERIC_INDEX]);
blesc_model_server_t * m_prime_server = &(m_blesc_models.server[BLESC_MODEL_PRIME_INDEX]);

/**@brief Function for resetting the mesh stack of BLESc.
 */
static void node_reset(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "----- Node reset  -----\n");
    /* This function may return if there are ongoing flash operations. */
    mesh_stack_device_reset();
}

/**@brief Get application key bound to BLESc model.
 *
 * @return Returns a const pointer to the array that stores the application key.
 */
static const uint8_t *mesh_config_get_appkey() {
    dsm_handle_t appkey_handle, subnet_handle;
    nrf_mesh_secmat_t p_secmat;
    
    access_model_publish_application_get(m_blesc_models.server[BLESC_MODEL_PRIME_INDEX].model_handle, &appkey_handle);
    dsm_appkey_handle_to_subnet_handle(appkey_handle, &subnet_handle);
    dsm_tx_secmat_get(subnet_handle, appkey_handle, &p_secmat);

    return p_secmat.p_app->key;
}

/**@brief Callback for config server events.
 */
static void config_server_evt_cb(const config_server_evt_t * p_evt)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CONFIG evt: %x\r\n", p_evt->type);
    switch (p_evt->type) {
    case CONFIG_SERVER_EVT_NODE_RESET:
        node_reset();
        break;
    case CONFIG_SERVER_EVT_MODEL_SUBSCRIPTION_ADD: // last config step on provisioner
        config_complete_cb(mesh_config_get_appkey());
        break;
    }
}

/**@brief Callback for successful mesh node provisioning.
 */
static void provisioning_complete_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Successfully provisioned\n");

#if MESH_FEATURE_GATT_ENABLED
    /* Restores the application parameters after switching from the Provisioning
     * service to the Proxy  */
    gap_params_init();
    conn_params_init();
#endif

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Node Address: 0x%04x \n", mesh_get_node_address());
}

/**@brief Funtion for initialising BLESc app.
 */
static void models_init_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Initializing and adding models\n");
   
    /* Instantiate BLESc models */
    app_blesc_basic_init();
    for(uint8_t element_index = 0; BLESC_MODELS_AMOUNT > element_index; ++element_index) {
        ERROR_CHECK(app_blesc_init(&m_blesc_models, element_index));
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "App BLESc Model %d Handle: %d\n", element_index, m_blesc_models.server[element_index].model_handle);
    }
}

/**@brief Function for initializing mesh stack.
 */
static void mesh_init(void)
{
    mesh_stack_init_params_t init_params =
    {
        .core.irq_priority       = NRF_MESH_IRQ_PRIORITY_LOWEST,
        .core.lfclksrc           = DEV_BOARD_LF_CLK_CFG,
        .core.p_uuid             = NULL,
        .models.models_init_cb   = models_init_cb,
        .models.config_server_cb = config_server_evt_cb
    };
    ERROR_CHECK(mesh_stack_init(&init_params, &m_device_provisioned));
}

/**************************** Interface functions *********************************/

void mesh_main_init(mesh_main_config_complete_cb_t config_cb)
{
    __LOG_INIT(LOG_SRC_APP | LOG_SRC_FRIEND, APP_CONFIG_LOG_LEVEL, LOG_CALLBACK_DEFAULT);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "----- BLE Scanner mesh network -----\n");

#if MESH_FEATURE_GATT_ENABLED
    gap_params_init();
    conn_params_init();
#endif
    
    config_complete_cb = config_cb;

    mesh_init();
}

void mesh_main_start(void)
{
    if (!m_device_provisioned)
    {
        static const uint8_t static_auth_data[NRF_MESH_KEY_SIZE] = STATIC_AUTH_DATA;
        mesh_provisionee_start_params_t prov_start_params =
        {
            .p_static_data                       = static_auth_data,
            .prov_complete_cb                    = provisioning_complete_cb,
            .prov_device_identification_start_cb = NULL,
            .prov_device_identification_stop_cb  = NULL,
            .prov_abort_cb                       = NULL,
            .p_device_uri                        = EX_URI_BLESC
        };
        ERROR_CHECK(mesh_provisionee_prov_start(&prov_start_params));
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Node Address: 0x%04x\r\n", mesh_get_node_address());
        config_complete_cb(mesh_config_get_appkey());
    }

    const uint8_t *p_uuid = nrf_mesh_configure_device_uuid_get();
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Device UUID", p_uuid, NRF_MESH_UUID_SIZE);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MESH starting up\r\n");
    ERROR_CHECK(mesh_stack_start());
}

void mesh_main_mesh_enable(void) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MESH starting up\r\n");
    nrf_mesh_enable();
}
void mesh_main_mesh_disable(void) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MESH stopping\r\n");
    nrf_mesh_disable();
}

uint32_t mesh_main_send_message(blesc_model_event_t event_type, uint8_t uuid_index, uint8_t data_index) {
    uint32_t status = NRF_SUCCESS;
    static uint8_t tid = 0;

    switch (event_type) {
    case BLESC_MODEL_EVT_SENDING_RSSI_LEVELS: {
        uint8_t params_tid = tid++;
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Sending bleam_uuid and RSSI\r\n");

        status = blesc_model_rssi_publish(m_generic_server, params_tid, uuid_index, data_index);
        
        break;
    }
    case BLESC_MODEL_EVT_HEALTH_STATUS: {
        blesc_model_event_params_t event_params;

        uint8_t params_tid = tid++;

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Sending health status\r\n");
        status = blesc_model_health_publish(m_generic_server, params_tid, data_index);
        break;
    }
    case BLESC_MODEL_EVT_PRIME_REMINDER: {
        blesc_model_event_params_t event_params;

        uint8_t params_tid = tid++;

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Sending prime reminder\r\n");
        status = blesc_model_prime_reminder_send(m_prime_server, params_tid, mesh_get_node_address());
        break;
    }}

    switch (status) {
    case NRF_SUCCESS:
        break;
    case NRF_ERROR_NO_MEM:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Cannot send data; ERROR: NO MEM\n");
        break;
    case NRF_ERROR_BUSY:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Cannot send data; ERROR: BUSY\n");
        break;
    case NRF_ERROR_INVALID_STATE:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Cannot send data; ERROR: INVALID STATE\n");
        status = NRF_SUCCESS;
        break;

    case NRF_ERROR_INVALID_PARAM:
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Cannod send data; ERROR: INVALID_PARAM\n");
        break;

    default:
        ERROR_CHECK(status);
        break;
    }

    return status;
}

uint32_t set_generic_model_publish_address(uint16_t publish_address) 
{  
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Setting model publish address as %04x at model %04x\r\n", publish_address, m_generic_server->model_handle);
    uint32_t status = NRF_SUCCESS;
    dsm_handle_t publish_address_handle = DSM_HANDLE_INVALID;
    status = access_model_publish_address_get(m_generic_server->model_handle, &publish_address_handle);
    NRF_MESH_ASSERT(status == NRF_SUCCESS);
    status = dsm_address_publish_remove(publish_address_handle);
    NRF_MESH_ASSERT(status == NRF_SUCCESS);
    status = dsm_address_publish_add(publish_address+1, &publish_address_handle); 
    if (status != NRF_SUCCESS) {
        return status;
    } 
    else {
        return access_model_publish_address_set(m_generic_server->model_handle, publish_address_handle);
    }
}

void mesh_main_button_event_handler(uint32_t button_number) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Button %u pressed\r\n", button_number + 1);

    /* Clear all the states to reset the node. */
    if (mesh_stack_is_device_provisioned()) {
#if MESH_FEATURE_GATT_PROXY_ENABLED
        (void)proxy_stop();
#endif
        mesh_stack_config_clear();
        node_reset();
//        blesc_model_publish_to_prime(m_server, mesh_get_node_address());
    }
    else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Node is unprovisioned. Resetting has no effect.\r\n");
    }
}

uint16_t mesh_get_node_address(void) {
    dsm_local_unicast_address_t addr;
    dsm_local_unicast_addresses_get(&addr);

    if (NRF_MESH_ADDR_UNASSIGNED == addr.address_start)
        return NULL;
    return addr.address_start;
}

void mesh_app_blesc_reelect_prime(void) {
    config_complete_cb(NULL);
}