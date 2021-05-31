/**
 * @addtogroup config_service
 * @{
 */
#ifndef CONFIG_S_H__
#define CONFIG_S_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "global_app_config.h"
#if defined(SDK_15_3)
  #include "nrf_sdh_ble.h"
  #include "nrf_crypto_rng.h"
#endif
#if defined(SDK_12_3)
  #include "nrf_drv_rng.h"
#endif

#define APP_ADV_INTERVAL     800                                   /**< The advertising interval (in units of 0.625 ms. This value corresponds to 500 ms). */
#define APP_ADV_DURATION     BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED /**< Disable advertising timeout. */
#define DEVICE_NAME_MAX_SIZE 20                                    /**< Maximum size of the device name string. */

/**@brief Configuration server instance structure. */
typedef struct config_s_server_s config_s_server_t;

#define CONFIG_S_UUID       APP_CONFIG_CONFIG_SERVICE_UUID         /**< 16-bit UUID of Configuration Service. */
#define CONFIG_S_BASE_UUID {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, \
                            0x00, 0x00, \
                            0x00, 0x00, \
                            0x00, 0x00, \
                            (uint8_t)(0xFF & CONFIG_S_UUID), (uint8_t)(CONFIG_S_UUID >> 8), \
                            0x00, 0x00}                            /**< Base UUID for Configuration Service. */
#define CONFIG_S_CHARS_NUM  8                                      /**< Number of Configuration Service characteristics */

/**@brief Configuration Service characterisctic IDs. */
typedef enum {
    CONFIG_S_VERSION = 1,  /**< Version characteristic. [READ] */
    CONFIG_S_STATUS,       /**< Status characteristic. [READ WRITE NOTIFY] */
    CONFIG_S_BLESC_PUBKEY, /**< Bleam Scanner public key characteristic. [READ NOTIFY] */
    CONFIG_S_BLEAM_PUBKEY, /**< Bleam public key characteristic. [WRITE] */
    CONFIG_S_NODE_ID,      /**< Node address characteristic. [WRITE] */
} config_s_char_t;

/**@brief Configuration Service characterisctic IDs. */
typedef enum {
    CONFIG_S_MSG_SIZE_VERSION      = 4,                                /**< Version message size */
    CONFIG_S_MSG_SIZE_STATUS       = 1,                                /**< Status characteristic. [READ WRITE NOTIFY] */
    CONFIG_S_MSG_SIZE_BLESC_PUBKEY = (APP_CONFIG_DATA_CHUNK_SIZE + 1), /**< Bleam Scanner public key characteristic. [READ NOTIFY] */
    CONFIG_S_MSG_SIZE_BLEAM_PUBKEY = (APP_CONFIG_DATA_CHUNK_SIZE + 1), /**< Bleam public key characteristic. [WRITE] */
    CONFIG_S_MSG_SIZE_NODE_ID      = sizeof(uint16_t),                 /**< Node address characteristic. [WRITE] */
} config_s_msg_size_t;

/**@brief Configuration Service status type. */
typedef enum {
    CONFIG_S_STATUS_WAITING, /**< Waiting for Bleam public key and node ID from API */
    CONFIG_S_STATUS_SET,     /**< Bleam public key and node ID are set */
    CONFIG_S_STATUS_DONE,    /**< Bleam Scanner node configured and saved to server */
    CONFIG_S_STATUS_FAIL,    /**< Configuring node failed for some reason */
} config_s_status_t;

/**@brief Configuration Service event type. */
typedef enum {
    CONFIG_S_SERVER_EVT_CONNECTED,    /**< Event indicating that the Configuration Service has been discovered at the peer. */
    CONFIG_S_SERVER_EVT_DISCONNECTED, /**< Event indicating that the Configuration Service has been disconnected from the peer. */
    CONFIG_S_SERVER_EVT_WRITE,        /**< Event indicating that the Configuration Service has been written to. */
    CONFIG_S_SERVER_EVT_PUBLISH,      /**< Event indicating that the Configuration Service has finished publishing data. */
    CONFIG_S_SERVER_EVT_FAIL,         /**< Event indicating that Bleam Scanner has failed to be configured. */
} config_s_server_evt_type_t;

#if defined(SDK_15_3)
  #define CONFIG_S_SERVER_BLE_OBSERVER_PRIO 3 /**< Priority of the observer event handler for confuguration service. */
/**@brief Macro for defining a configuration service server instance
 *        and registering event observer.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
  #define CONFIG_S_SERVER_DEF(_name)         \
    static config_s_server_t _name;        \
    NRF_SDH_BLE_OBSERVER(_name##_obs,          \
        CONFIG_S_SERVER_BLE_OBSERVER_PRIO, \
        config_s_server_on_ble_evt, &_name)
#endif
#if defined(SDK_12_3)
  #define CONFIG_S_SERVER_DEF(_name)         \
    static config_s_server_t _name; /**< Configuration Service server instance */
#endif

/**@brief Configuration Event structure. */
typedef struct {
    config_s_server_evt_type_t      evt_type;       /**< Type of the event. */
    uint16_t                      conn_handle;      /**< Connection handle on which the event occured.*/
    ble_gatts_evt_write_t const * write_evt_params; /**< Write event params. */
} config_s_server_evt_t;

/**@brief Configuration Service event handler type. */
typedef void (*config_s_server_evt_handler_t) (config_s_server_t *p_config_s_server, config_s_server_evt_t const *p_evt);

/**@brief Configuration Service server initialization structure. */
typedef struct
{
    config_s_server_evt_handler_t  evt_handler;  /**< Event handler to be called by the Configuration Service server module whenever there is an event related to the Configuration Service. */
    ble_srv_cccd_security_mode_t   char_attr_md; /**< Initial security level for Custom characteristics attribute */
} config_s_server_init_t;

struct config_s_server_s {
    uint16_t                      conn_handle;                      /**< Connection handle as provided by the SoftDevice. */
    uint16_t                      service_handle;                   /**< Handle of Configuration Service (as provided by the BLE stack). */
    config_s_server_evt_handler_t evt_handler;                      /**< Application event handler to be called when there is an event related to the Configuration Service. */
    ble_gatts_char_handles_t      char_handles[CONFIG_S_CHARS_NUM]; /**< Handles of characteristics as provided by the SoftDevice. */
    uint8_t                       uuid_type;                        /**< UUID type. */
};

/**@brief Version data structure.
 *
 *@note This struct has _packed_ attribute
 */
typedef struct __attribute((packed)) {
    uint8_t  protocol_id; /**< Protocol number. */
    uint16_t fw_id;       /**< Firmware version number. */
    uint8_t  hw_id;       /**< Hardware ID. */
} config_s_server_version_t;

/**@brief Function for handling ble events for Configuration service
 *
 * @param[in]    p_ble_evt              Pointer to event received from the BLE stack.
 * @param[in]    p_context              Pointer to the struct of Configuration Service.
 *
 * @returns Nothing.
 */
void config_s_server_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief Function for initialising the Configuration Service
 *
 * @details Function initialises the Configuration Service.
 *
 * @param[in]    p_config_s_server        Pointer to the struct of Configuration Service.
 * @param[in]    p_config_s_server_init   Pointer to the struct storing the Configuration Service init params.
 *
 * @retval       NRF_SUCCESS if all Configuration service characteristics have been successfully created.
 * @retval       NRF_ERROR_NULL if any of the parameter pointers is NULL.
 * @returns a bitwise OR of other errors returned by @ref config_s_char_add() calls.
 */
uint32_t config_s_server_init(config_s_server_t *p_config_s_server, config_s_server_init_t *p_config_s_server_init);

/**@brief Function for updating the status.
 *
 * @details The application calls this function when the custom value should be updated.
 *          If notification has been enabled, the custom value characteristic is sent to the client.
 *       
 * @param[in]    p_config_s_server       Pointer to the struct of Configuration Service.
 * @param[in]    p_status                Node configuration status to be reported to Bleam.
 *
 * @retval       NRF_SUCCESS is status is updated successfully.
 * @retval       NRF_ERROR_NULL if the parameter pointer is NULL.
 * @retval       NRF_ERROR_INVALID_STATE if connection handle is invalid.
 * @returns otherwise the return value of SDK 15.3.0 @link_sd_ble_gatts_hvx or SDK 12.3.0 @link_12_sd_ble_gatts_hvx.
 */
uint32_t config_s_status_update(config_s_server_t *p_config_s_server, config_s_status_t p_status);

/**@brief Function for publishing Bleam Scanner version data.
 *
 * @details The application calls this function on config mode startup.
 *          Bleam Toolds app will read version data on connect to determine Bleam Scanner protocol and hardware.
 *       
 * @param[in]    p_config_s_server       Pointer to the struct of Configuration Service.
 *
 * @retval       NRF_SUCCESS version info is sent successfully.
 * @retval       NRF_ERROR_NULL if the parameter pointer is NULL.
 * @retval       NRF_ERROR_INVALID_STATE if connection handle is invalid.
 * @returns otherwise the return value of SDK 15.3.0 @link_sd_ble_gatts_value_set or SDK 12.3.0 @link_12_sd_ble_gatts_value_set.
 */
uint32_t config_s_publish_version(config_s_server_t *p_config_s_server);

/**@brief Function for publishing Bleam Scanner public key.
 *
 * @details The application calls this function when the cutom value that should be updated.
 *          If notification has been enabled, the custom value characteristic is sent to the client.
 *       
 * @param[in]    p_config_s_server       Pointer to the struct of Configuration Service.
 * @param[in]    chunk_number            Number of the data chunk that is being sent.
 * @param[in]    pubkey_chunk            Pointer to data chunk to be sent.
 *
 * @retval       NRF_SUCCESS version info is sent successfully.
 * @retval       NRF_ERROR_NULL if the parameter pointer is NULL.
 * @retval       NRF_ERROR_INVALID_PARAM if chunk_number is invalid.
 * @retval       NRF_ERROR_INVALID_STATE if connection handle is invalid.
 * @returns otherwise the return value of SDK 15.3.0 @link_sd_ble_gatts_value_set or @link_sd_ble_gatts_hvx or
 *          SDK 12.3.0 @link_12_sd_ble_gatts_value_set or @link_12_sd_ble_gatts_hvx.
 */
uint32_t config_s_publish_pubkey_chunk(config_s_server_t *p_config_s_server, uint8_t chunk_number, uint8_t * pubkey_chunk);

/**@brief Get Configuration service current status.
 *
 * @returns Configuration service current status.
 */
config_s_status_t config_s_get_status(void);

/**@brief Function for finishing configuration process.
 *
 * @returns Nothing.
 */
void config_s_finish(void);

#endif // CONFIG_S_H__

/** @}*/