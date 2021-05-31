/**
 * @addtogroup bleam_service
 * @{
 */

#ifndef BLEAM_SERVICE_H__
#define BLEAM_SERVICE_H__

#include <stdint.h>
#include <stdbool.h>
#include "blesc_error.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "ble_db_discovery.h"
#include "global_app_config.h"
#include "app_timer.h"
#if defined(SDK_15_3)
  #include "nrf_sdh_ble.h"
#endif

/* Forward declaration of the bleam_service_client_s type. */
typedef struct bleam_service_client_s bleam_service_client_t;

#define BLEAM_SERVICE_UUID                      APP_CONFIG_BLEAM_SERVICE_UUID                        /**< UUID of Bleam device. */
#define BLE_UUID_BLEAM_SERVICE_BASE_UUID       {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, \
                                                0x00, 0x00, \
                                                0x00, 0x00, \
                                                (uint8_t)(0xFF & BLEAM_SERVICE_UUID), (uint8_t)(BLEAM_SERVICE_UUID >> 8), \
                                                0x00, 0x00}                                          /**< Base UUID for BLEAMs */
#define BLEAM_SERVICE_BLEAM_INACTIVITY_TIMEOUT  __TIMER_TICKS(APP_CONFIG_BLEAM_INACTIVITY_TIMEOUT)   /**< Duration of allowed Bleam inactivity. */
#define BLEAM_MAX_DATA_LEN                     (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - 3)                   /**< Maximum length of data to send to Bleam */

/**@brief Bleam Service characterisctic IDs. 
 *
 * @note Main members of this enumeration start counting up after @ref BLEAM_SERVICE_UUID.
 *       "Empty" and "final" values are helper values, and not actual characteristic UUIDs.
 */
typedef enum {
    BLEAM_S_NOTIFY = BLEAM_SERVICE_UUID + 1, /**< Notify characteristic. [NOTIFY,READ] */
    BLEAM_S_SIGN,                            /**< Signature for Bleam or Salt from Bleam characteristic. [WRITE] */
    BLEAM_S_RSSI,                            /**< RSSI data characteristic. [WRITE] */
    BLEAM_S_HEALTH,                          /**< Health status characteristic. [WRITE] */
    BLEAM_S_TIME,                            /**< Local Bleam time characteristic. [READ] */
    BLEAM_S_MAC,                             /**< Bleam Scanner info characteristic. [WRITE] */
    BLEAM_CHAR_EMPTY = 0x0000,               /**< Value of empty char variable */
    BLEAM_CHAR_FINAL = 0xFFFF,               /**< Value of char variable to flag the sending finalized */
} bleam_service_char_t;

/**@brief Bleam RSSI data structure. */
typedef struct {
    int8_t  rssi; /**< Received Signal Strength of Bleam */
    uint8_t aoa;  /**< Angle of arrival of Bleam signal */
} bleam_service_rssi_data_t;

/** @brief Health general data struct
 */
typedef struct __attribute((packed)) {
    uint8_t       msg_type;    /**< Flag that signifies this is a general health message. Always should be 0x01 */
    uint8_t       battery_lvl; /**< Battery level in centivolts */
    uint16_t      fw_id;       /**< Bleam Scanner firmware version number */
    uint32_t      uptime;      /**< Node uptime in minutes */
    uint32_t      system_time; /**< Bleam Scanner system time in seconds passed since midnight */
    uint16_t      err_id;      /**< Latest error ID that is randomly generated. */
    blesc_error_t err_type;    /**< Latest error type @ref blesc_error_t */
    uint32_t      sleep_time;  /**< Amount of time Bleam Scanner node has slept since previous Bleam connection. */
} bleam_service_health_general_data_t;

/** @brief Health error info struct
 */
typedef struct __attribute((packed)) {
    uint8_t  msg_type;                            /**< Flag that signifies this is a detailed error info message. Always should be 0x02 */
    uint32_t err_code;                            /**< The error code representing the error that occurred (from nRF SDK). */
    uint16_t line_num;                            /**< The line number where the error occurred. */
    uint8_t  file_name[BLESC_ERR_FILE_NAME_SIZE]; /**< The file in which the error occurred (first 13 symbols) */
} bleam_service_health_error_info_t;

/** @brief MAC info struct
 */
typedef struct  __attribute((packed)) {
    uint8_t  mac[BLE_GAP_ADDR_LEN]; /**< Bleam Scanner node MAC address. */
    uint16_t node_id;               /**< Bleam Scanner node ID. */
} bleam_service_mac_info_t;

/**@brief Bleam Service type, signifying possible type of future interaction. */
typedef enum {
    BLEAM_SERVICE_TYPE_AOS   = 0x00, /**< Regular Bleam on Android OS */
    BLEAM_SERVICE_TYPE_TOOLS = 0x01, /**< Bleam Tools */
    BLEAM_SERVICE_TYPE_BKGD  = 0x02, /**< Background Bleam process */
    BLEAM_SERVICE_TYPE_IOS   = 0xFF, /**< Regular Bleam on iOS */
} bleam_service_type_t;

/**@brief Bleam Service message data sizes. */
typedef enum {
    BLEAM_S_MSG_SIZE_NOTIFY = (APP_CONFIG_DATA_CHUNK_SIZE + 2),            /**< Action byte + Chunk number byte + Data. */
    BLEAM_S_MSG_SIZE_SIGN   = (APP_CONFIG_DATA_CHUNK_SIZE + 1),            /**< Chunk number byte + Data chunk. */
    BLEAM_S_MSG_SIZE_RSSI   = BLEAM_MAX_DATA_LEN,                          /**< RSSI data (max size). */
    BLEAM_S_MSG_SIZE_HEALTH = sizeof(bleam_service_health_general_data_t), /**< General health status. */
    BLEAM_S_MSG_SIZE_ERROR  = sizeof(bleam_service_health_error_info_t),   /**< Error info. */
    BLEAM_S_MSG_SIZE_TIME   = sizeof(uint32_t),                            /**< Local Bleam time. */
    BLEAM_S_MSG_SIZE_MAC    = sizeof(bleam_service_mac_info_t),            /**< Bleam Scanner info. */
} bleam_service_msg_size_t;

#if defined(SDK_15_3)
  #define BLEAM_SERVICE_CLIENT_BLE_OBSERVER_PRIO 2 /**< Service's BLE observer priority. */
  
/**@brief Macro for defining a bleam_service_client instance
 *        and registering event observer.
 *
 * @param _name Name of the instance.
 * @hideinitializer
 */
  #define BLEAM_SERVICE_CLIENT_DEF(_name)         \
    static bleam_service_client_t _name;        \
    NRF_SDH_BLE_OBSERVER(_name##_obs,           \
        BLEAM_SERVICE_CLIENT_BLE_OBSERVER_PRIO, \
        bleam_service_client_on_ble_evt, &_name)
#endif
#if defined(SDK_12_3)
  #define BLEAM_SERVICE_CLIENT_DEF(_name) \
    static bleam_service_client_t _name; /**< Bleam sevice client instance. */
#endif

/**@brief Bleam Service event type. */
typedef enum {
    BLEAM_SERVICE_CLIENT_EVT_NOTIFICATION_ENABLED,   /**< Notification enabled event. */
    BLEAM_SERVICE_CLIENT_EVT_NOTIFICATION_DISABLED,  /**< Notification disabled event. */
    BLEAM_SERVICE_CLIENT_EVT_DISCOVERY_COMPLETE,     /**< Discovered at the peer event. */
    BLEAM_SERVICE_CLIENT_EVT_SRV_NOT_FOUND,          /**< Service is not found at the peer event. */
    BLEAM_SERVICE_CLIENT_EVT_BAD_CONNECTION,         /**< Service discovery failed, or other connecion error to the peer event. */
    BLEAM_SERVICE_CLIENT_EVT_DISCONNECTED,           /**< Disconnected from the peer event. */
    BLEAM_SERVICE_CLIENT_EVT_CONNECTED,              /**< Connected to the peer event. */
    BLEAM_SERVICE_CLIENT_EVT_PUBLISH,                /**< Published to the peer event. */
    BLEAM_SERVICE_CLIENT_EVT_RECV_SALT,              /**< Read salt from peer event. */
    BLEAM_SERVICE_CLIENT_EVT_RECV_TIME,              /**< Read time from peer event. */
    BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_SIGNATURE, /**< Done sending signature to peer event. */
    BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_HEALTH,    /**< Done sending health to peer event. */
    BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_RSSI,      /**< Done sending RSSI data to peer event. */
} bleam_service_client_evt_type_t;

/**@brief Bleam Service mode type, signifying protocol of Bleam Scanner-Bleam interation. */
typedef enum {
    BLEAM_SERVICE_CLIENT_MODE_NONE, /**< No mode has been set for this connection. */
    BLEAM_SERVICE_CLIENT_MODE_RSSI, /**< Generic send-RSSI-to-Bleam interaction. */
    BLEAM_SERVICE_CLIENT_MODE_CMD,  /**< Confirm Bleam is genuine and execute action command. */
} bleam_service_client_mode_type_t;

/**@brief Bleam Service command type, value received within the salt package. */
typedef enum {
    BLEAM_SERVICE_CLIENT_CMD_SALT       = 0x00, /**< Received salt for Bleam RSSI interaction, ready to accept signature from Bleam Scanner. */
    BLEAM_SERVICE_CLIENT_CMD_TRUST      = 0x10, /**< Received command to skip sending signature and start sending HEALTH and RSSI data. */
    BLEAM_SERVICE_CLIENT_CMD_SIGN       = 0x01, /**< Received a chunk of signature from Bleam. */
    BLEAM_SERVICE_CLIENT_CMD_DFU        = 0x02, /**< Received command for entering DFU, ready to accept salt from Bleam Scanner. */
    BLEAM_SERVICE_CLIENT_CMD_REBOOT     = 0x03, /**< Received command for node reboot, ready to accept salt from Bleam Scanner. */
    BLEAM_SERVICE_CLIENT_CMD_UNCONFIG   = 0x04, /**< Received command for node unconfiguration, ready to accept salt from Bleam Scanner. */
    BLEAM_SERVICE_CLIENT_CMD_IDLE       = 0x05, /**< Received command to IDLE, ready to accept salt from Bleam Scanner. */
    BLEAM_SERVICE_CLIENT_CMD_RSSI_LIMIT = 0x06, /**< Received command to set the new lower limit of RSSI for accepting advertising packets from Bleam. */
} bleam_service_client_cmd_type_t;

/**@brief Structure containing the handles related to the Bleam Service found on the peer. */
typedef struct {
    uint16_t salt_handle;      /**< Handle of the NOTIFY read characteristic as provided by the SoftDevice. */
    uint16_t salt_cccd_handle; /**< Handle of the NOTIFY notify characteristic as provided by the SoftDevice. */
    uint16_t signature_handle; /**< Handle of the SIGN characteristic as provided by the SoftDevice. */
    uint16_t rssi_handle;      /**< Handle of the RSSI characteristic as provided by the SoftDevice. */
    uint16_t health_handle;    /**< Handle of the HEALTH characteristic as provided by the SoftDevice. */
    uint16_t time_handle;      /**< Handle of the TIME characteristic as provided by the SoftDevice. */
    uint16_t mac_handle;       /**< Handle of the MAC characteristic as provided by the SoftDevice. */
} bleam_service_db_t;

/**@brief Bleam Event structure. */
typedef struct {
    bleam_service_client_evt_type_t evt_type; /**< Type of the event. */
    uint16_t conn_handle;                     /**< Connection handle on which the event occured.*/
    bleam_service_db_t handles;               /**< Handles found on the peer device. This will be filled if the evt_type is @ref BLEAM_SERVICE_CLIENT_EVT_DISCOVERY_COMPLETE.*/
    uint16_t data_len;                        /**< Length of data received. This will be filled if the ext_type is @ref BLEAM_SERVICE_CLIENT_EVT_RECV_SALT or @ref BLEAM_SERVICE_CLIENT_EVT_RECV_TIME. */
    uint8_t *p_data;                          /**< Data received. This will be filled if the ext_type is @ref BLEAM_SERVICE_CLIENT_EVT_RECV_SALT or @ref BLEAM_SERVICE_CLIENT_EVT_RECV_TIME. */
} bleam_service_client_evt_t;

/**@brief Bleam Service event handler type. */
typedef void (*bleam_service_client_evt_handler_t) (bleam_service_client_t * p_bas, bleam_service_client_evt_t * p_evt);

/**@brief Bleam Service Client initialization structure. */
typedef struct
{
    bleam_service_client_evt_handler_t evt_handler; /**< Event handler to be called by the Bleam Service Client module whenever there is an event related to the Bleam Service. */
} bleam_service_client_init_t;

struct bleam_service_client_s {
    uint16_t conn_handle;                           /**< Connection handle as provided by the SoftDevice. */
    bleam_service_db_t handles;                     /**< Handles related to Bleam Service on the peer*/
    bleam_service_client_evt_handler_t evt_handler; /**< Application event handler to be called when there is an event related to the Bleam service. */
    uint8_t uuid_type;                              /**< UUID type. */
};

/**@brief Function for initialising the Bleam service
 *
 * @param[in] p_bleam_service_client            Pointer to the struct of Bleam service.
 * @param[in] p_bleam_service_client_init       Pointer to the struct storing the Bleam service init params.
 * @param[in] cb                                Pointer to the function to reset softdevice.
 *
 * @retval NRF_SUCCESS if Bleam service is init.
 * @retval NRF_ERROR_NULL if any of the parameter pointers is NULL.
 * @returns otherwise, an error code of @link_sd_ble_uuid_vs_add or @link_ble_db_discovery_evt_register call for SDK 15.3.0
 *          or @link_12_sd_ble_uuid_vs_add or @link_12_ble_db_discovery_evt_register for SDK 12.3.0.
 */
uint32_t bleam_service_client_init(bleam_service_client_t *p_bleam_service_client,
                                   bleam_service_client_init_t *p_bleam_service_client_init,
                                   void (* cb)(void));

/**@brief Function for changing Bleam service base UUID in the BLE stack's table
 *
 *@details Function removes previously added Bleam service vendor specific UUID from the
 *         BLE stack's table and addn a new one, in order for DB discovery to find
 *         the service on the new Bleam device.
 *
 * @param[in] p_bleam_service_client            Pointer to the struct of Bleam service.
 * @param[in] bleam_service_base_uuid           Pointer to the struct storing the Bleam base UUID.
 *
 * @return NRF_SUCCESS if Bleam service base UUID in BLE stack table is replaced successfully.
 * @returns otherwise, a non-zero error code of either @link_sd_ble_uuid_vs_remove or @link_sd_ble_uuid_vs_add calls
 *          for SDK 15.3.0, or @link_12_sd_ble_uuid_vs_add for SDK 12.3.0.
 */
uint32_t bleam_service_uuid_vs_replace(bleam_service_client_t *p_bleam_service_client, ble_uuid128_t *bleam_service_base_uuid);

/**@brief Function for handling ble events for Bleam service
 *
 * @param[in] p_ble_evt                         Pointer to event received from the BLE stack.
 * @param[in] p_context                         Pointer to the struct of Bleam service.
 *
 * @returns Nothing.
 */
void bleam_service_client_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**@brief Function for handling DB discovery events.
 *
 * @param[in] p_bleam_service_client            Pointer to the struct of Bleam service.
 * @param[in] p_evt                             Pointer to event received from DB discovery
 *
 * @returns Nothing.
 */
void bleam_service_on_db_disc_evt(bleam_service_client_t *p_bleam_service_client, const ble_db_discovery_evt_t *p_evt);

/**@brief Function for assigning handles to Bleam service
 *
 * @details Function assigns new conn handle for the service and peer handles for characteristics.
 *
 * @param[in] p_bleam_service_client            Pointer to the struct of Bleam service.
 * @param[in] conn_handle                       Pointer to the struct storing conn handle.
 * @param[in] p_peer_handles                    Pointer to the struct storing peer handles.
 *
 * @retval NRF_SUCCESS if handlers are assigned successfully.
 * @retval NRF_ERROR_NULL if any of the parameter pointers is NULL.
 */
uint32_t bleam_service_client_handles_assign(bleam_service_client_t *p_bleam_service_client,
    uint16_t conn_handle,
    const bleam_service_db_t *p_peer_handles);

/**@brief Function for requesting the peer to start sending notification of NOTIFY characteristic.
 *
 * @param[in]   p_bleam_service_client       Pointer to the struct of Bleam service.
 *
 * @retval NRF_SUCCESS if handlers are assigned successfully.
 * @retval NRF_ERROR_NULL if any of the parameter pointers is NULL.
 * @retval NRF_ERROR_INVALID_STATE if either the connection state or NOTIFY characteristic handle is invalid.
 * @returns otherwise, the return value of SDK 15.3.0 @link_sd_ble_gattc_write or SDK 12.3.0 @link_12_sd_ble_gattc_write call.
 */
uint32_t bleam_service_client_notify_enable(bleam_service_client_t *p_bleam_service_client);

/**@brief Function for reading the peer TIME characteristic.
 *
 * @param[in]   p_bleam_service_client       Pointer to the struct of Bleam service.
 *
 * @retval NRF_SUCCESS if time is read successfully.
 * @retval NRF_ERROR_NULL if any of the parameter pointers is NULL.
 * @retval NRF_ERROR_INVALID_STATE if the connection state is invalid.
 * @retval NRF_ERROR_NOT_FOUND if the TIME characteristic is not fount at the peer.
 * @returns otherwise, an error code of SDK 15.3.0 @link_sd_ble_gattc_read or SDK 12.3.0 @link_12_sd_ble_gattc_read call.
 */
uint32_t bleam_service_client_read_time(bleam_service_client_t *p_bleam_service_client);

/**@brief Function for writing data to the Bleam service
 *
 * @param[in] p_bleam_service_client            Pointer to the struct of Bleam service.
 * @param[in] data_array                        Pointer to the data buffer.
 * @param[in] data_size                         Data buffer length.
 * @param[in] write_handle                      Characteristic to write data to @ref bleam_service_char_t.
 *
 * @retval NRF_ERROR_NULL          if any pointer is NULL.
 * @retval NRF_ERROR_INVALID_PARAM if any parameter does not match its expected value.
 * @retval NRF_ERROR_INVALID_STATE if Bleam Scanner node state does not match the expected state (connected to BLEAM).
 * @retval NRF_ERROR_NOT_FOUND     if Bleam Scanner attempts to write to handle that isn't found at peer.
 * @retval result of SDK 15.3.0 @link_sd_ble_gattc_write or SDK 12.3.0 @link_12_sd_ble_gattc_write call.
 */
uint32_t bleam_service_data_send(bleam_service_client_t *p_bleam_service_client, uint8_t *data_array, uint16_t data_size, uint16_t write_handle);

/**@brief Function for getting current Bleam service mode.
 *
 * @returns Bleam service current mode value.
 */
bleam_service_client_mode_type_t bleam_service_mode_get(void);

/**@brief Function for setting a new Bleam service mode.
 *
 * @param[in]   p_mode       Value the Bleam service mode is to be set.
 *
 * @returns Nothing.
 */
void bleam_service_mode_set(bleam_service_client_mode_type_t p_mode);

#endif // BLEAM_SERVICE_H__

/** @}*/