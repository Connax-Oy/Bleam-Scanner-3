/**
 * @addtogroup task_scan_connect
 * @{
 */
#ifndef SCAN_CONNECT_HANDLER_H__
#define SCAN_CONNECT_HANDLER_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

#include "nrf.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_delay.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_power.h"
#if defined(SDK_15_3)
  #include "nrf_sdh.h"
  #include "nrf_ble_scan.h"
  #include "nrf_sdh_ble.h"
  #include "nrf_sdh_soc.h"
#endif
#if defined(SDK_12_3)
  #include "softdevice_handler.h"
  #include "task_scan.h"
#endif

#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_conn_state.h"
#include "ble_db_discovery.h"
#include "bleam_discovery.h"
#include "ble_hci.h"

#include "bleam_service.h"
#include "bleam_discovery.h"
#include "task_storage.h"

/* Scan macros */
#define RSSI_FILTER_TIMEOUT            __TIMER_TICKS(APP_CONFIG_RSSI_FILTER_INTERVAL)     /**< Minimum time between received RSSI scans from one device. */
#define SCAN_CONNECT_TIME              __TIMER_TICKS(APP_CONFIG_SCAN_CONNECT_INTERVAL)    /**< Time for Bleam RSSI scan process. */
#define SCAN_INTERVAL                  0x00A0                                             /**< Determines scan interval in units of 0.625 millisecond. */
#define SCAN_WINDOW                    0x0050                                             /**< Determines scan window in units of 0.625 millisecond. */
#if defined(SDK_15_3)
  #define SCAN_DURATION                0x0000                                             /**< Timeout when scanning in units if 10 ms. 0x0000 disables timeout. */
  #define CONNECT_TIMEOUT              0x012C                                             /**< Timeout when connecting in units of 10 ms. 0x0000 disables timeout. */
#endif
#if defined(SDK_12_3)
  #define SCAN_DURATION                0x0000                                             /**< Timeout in seconds, 0x0000 disables timeout.. */
  #define CONNECT_TIMEOUT              0x0003                                             /**< Timeout in seconds, 0x0000 disables timeout.. */
#endif

#define BLESC_SCAN_TIME                __TIMER_TICKS((APP_CONFIG_ECO_SCAN_SECS * 1000))   /**< Time for Bleam Scanner to scan for BLEAMs between sleeps */

/** Bleam Scanner state */
typedef enum {
    BLESC_STATE_IDLE = 0x00, // 00 - no scan
    BLESC_STATE_SCANNING,    // 01 - scan
    BLESC_STATE_CONNECT,     // 10 - connect
    BLESC_STATE_INIT,
} blesc_state_t;

/* Adv data struct for process_scan_data() */
typedef struct {
    uint8_t                            *p_data;  /**< Pointer to data. */
    uint16_t                           data_len; /**< Length of data. */
} data_t;

/**@brief Function for providing external modules with the Bleam Scanner node state value.
 *
 * @returns Bleam Scanner node state value.
 */
blesc_state_t blesc_node_state_get(void);

/**@brief Function for updating the Bleam Scanner node state value from external modules.
 *
 * @param[in] new_state   New state value.
 *
 * @returns Nothing.
 */
void blesc_node_state_set(blesc_state_t new_state);

/**@brief Function for providing external modules with data on a currently connected Bleam device.
 *
 * @returns Pointer to a data structure with Bleam device data.
 */
blesc_model_rssi_data_t * get_connected_bleam_data(void);

/**@brief Function to start inactivity timer.
 *
 * @returns Nothing.
 */
void bleam_inactivity_timer_start(void);

/**@brief Function to stop inactivity timer.
 *
 * @returns Nothing.
 */
void bleam_inactivity_timer_stop(void);

/**@brief Function for external modules to access whether
 *        an unkown iOS device has been detected.
 * @ingroup ios_solution
 *
 * @retval true if @ref stupid_ios_data hosts valuable info
 * @retval false otherwise
 */
bool stupid_ios_data_active(void);

/**@brief Function to wipe data from stupid_ios_data structure.
 * @ingroup ios_solution
 *
 * @returns Nothing.
 */
void stupid_ios_data_clear(void);

/**@brief Function for handling the Eco timer timeout.
 *
 * @details This function will be called each time the eco timer expires.
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 *
 * @returns Nothing.
 */
void eco_timer_handler(void * p_context);

/**@brief Function to start scanning.
 *
 * @returns Nothing.
 */
void scan_start(void);

/**@brief Function to stop scanning.
 *
 * @returns Nothing.
 */
void scan_stop(void);

/**@brief Function for trying to connect to chosen Bleam device.
 *
 * @param[in] p_index    Index of chosen Bleam device data in storage.
 *
 * @returns Nothing.
 */
void try_bleam_connect(uint8_t p_index);

/**@brief Function for trying to connect to chosen Bleam device on iOS
 *        with Bleam service running in the background.
 * @ingroup ios_solution
 *
 * @returns Nothing.
 */
void try_ios_connect();

/**@brief Function for handling the scan-connect timer timeout
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 *
 * @returns Nothing.
 */
void scan_connect_timer_handle(void *p_context);

/**@brief Function for handling received device adv data.
 * @ingroup ios_solution
 *
 * @details Function pulls device details from adv report. If the found device has Bleam service UUID,
 *          store the found device UUID and address and send the RSSI data.
 *
 * @param[in] p_adv_report            Pointer to the adv report.
 *
 * @returns Nothing.
 */
void process_scan_data(ble_gap_evt_adv_report_t const *p_adv_report);

/**@brief Function for initializing the Scan module.
 *
 * @param[in] p_db_disc               Pointer to the database discovery event.
 * @param[in] p_scan                  Pointer to the scan module instance.
 *
 * @returns Nothing.
 */
void scan_connect_init(ble_db_discovery_t     * p_db_disc,
                       nrf_ble_scan_t         * p_scan);

/**@brief Handle BLE connect to Bleam Tools for configuration.
 *
 * @param[in] p_ble_evt    Pointer to BLE event data.
 * @param[in] p_qwr        Pointer to QWR instance.
 *
 * @returns Nothing.
 */
void handle_connect_config(ble_evt_t const *p_ble_evt, nrf_ble_qwr_t * p_qwr);

/**@brief Handle BLE connect to iOS device to search for Bleam service.
 * @ingroup ios_solution
 *
 * @param[in] p_ble_evt    Pointer to BLE event data.
 *
 * @returns Nothing.
 */
void handle_connect_ios(ble_evt_t const *p_ble_evt);

/**@brief Handle BLE connect to Bleam device to share RSSI scan data.
 *
 * @param[in] p_ble_evt    Pointer to BLE event data.
 * @param[in] p_qwr        Pointer to QWR instance.
 *
 * @returns Nothing.
 */
void handle_connect_bleam(ble_evt_t const *p_ble_evt, nrf_ble_qwr_t * p_qwr);

/**@brief Wrapped for @ref bleam_connection_abort().
 *
 * @returns Nothing.
 */
void handle_connection_abort();

/**@brief Handle BLE disconnect.
 *
 * @returns Nothing.
 */
void handle_disconnect(void);

/**@brief Function for initializing services that will be used by configured Bleam Scanner.
 *
 * @param[in] p_bleam_service_client  Pointer to the Bleam service client instance.
 * @param[in] cb                      Pointer to the function to reset softdevice.
 *
 * @returns Nothing.
 */
void blesc_services_init(bleam_service_client_t * p_bleam_service_client, void (* cb)(void));

/**@brief Function for handling database discovery events.
 *
 * @details This function is a callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function forwards the events
 *          to their respective services.
 *
 * @param[in] p_evt    Pointer to the database discovery event.
 *
 * @returns Nothing.
 */
void db_disc_handler(ble_db_discovery_evt_t *p_evt);

/* Bleam service discovery */

/**@brief Function for handling the Bleam service custom discovery events.
 * @ingroup ios_solution
 *
 * @param[in] p_evt    Pointer to the Bleam service discovery event.
 */
void bleam_service_discovery_evt_handler(const bleam_service_discovery_evt_t *p_evt);

#endif // SCAN_CONNECT_HANDLER_H__

/** @}*/