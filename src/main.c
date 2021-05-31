/** @file main.c */

/************* Doxygen modules descriptions *************/

/**
 * @defgroup blesc_tasks Bleam Scanner modules
 * @brief Bleam Scanner modules.
 *
 * @details Bleam Scanner functions and definitions sorted into isolated modules.
 */

/**
 * @defgroup blesc_config Node configuration
 * @brief Bleam Scanner node configuration.
 *
 * @details Bleam Scanner node needs to identify itself within a Bleam system and
 *          obtain a key to help confirm its identity on data exchange with Bleam.
 *          Configuration is a process of unconfigured Bleam Scanner node receiving
 *          a node ID and an application key.
 *          For details, please refer to @link_wiki_config.
 */

/**
 * @defgroup bleam_connect Connecting to Bleam
 * @brief Connection to Bleam devices.
 *
 * @details Bleam Scanner connects to Bleam device in order to share the scanned data.
 *          For details, please refer to @link_wiki_connect.
 */

/**
 * @defgroup bleam_storage Storage of Bleam data
 * @brief Data structures that hold scanned Bleam data and functions that operate the structures.
 */

/**
 * @defgroup bleam_time Bleam system time
 * @brief Support of system time on a Bleam Scanner node.
 *
 * @details Bleam Scanner supports synced system time for better maintenance.
 *          For details, please refer to @link_wiki_time.
 */

/**
 * @defgroup ios_solution iOS problem solution
 * @brief Solution of iOS-specific background advertising problem.
 *
 * @details Apple devices don't explicitly advertise services in background;
 *          rather, they advertise Apple-specific info that is incomprehencible
 *          to third party BLE devices.
 *          This module is a solution to detecting Bleam service running
 *          in the background on an iOS device.
 *          For details, please refer to @link_wiki_ios.
 */

/**
 * @defgroup blesc_error Bleam Scanner custom error handler
 * @brief Bleam Scanner custom error handler
 *
 * @details Bleam scanner implements a custom error handler in order to
 *          **retain error data** after emergency reset and report it later within
 *          a health status.
 *          For details, please refer to @link_wiki_error.
 */

/**
 * @defgroup blesc_dfu Device Firmware Update interface
 * @brief nRF SDK @link_lib_bootloader operations and handlers for Bleam Scanner.
 *
 * @details Bleam Scanner supports bootloader in order to perform a DFU over BLE.
 *          Bleam Scanner is developed to be compatible with
 *          nRF52's @link_example_bootloader and nRF51's @link_12_example_bootloader.
 *          When developing for RuuviTag, refer to the tutorial @link_ruuvi_bootloader.
 */

/**
 * @defgroup blesc_app Other Bleam Scanner application members
 * @brief Softdevice, power manager and other important Bleam Scanner non-modules.
 */

/**
 * @defgroup blesc_debug Debug, LEDs and buttons
 * @brief Bleam Scanner debugging, button control and LED indication.
 *
 * @details For details, please refer to @link_wiki_debug.
 */

/**
 * @defgroup nrf51_handlers nRF51 system handlers
 * @brief System event handlers for nRF51 platform.
 *
 * @details nRF5 application architecture before nRF SDK 14 was based on
 *          manually written system event dispatch functions.
 *          Since SDK 14, as stated in @link_nrf_sdk_14_migration,
 *          SoftDevice uses registered observers to dispatch events.
 *          These functions are used in Bleam Scanner implementations
 *          for nRF51822 boards with SDK 12.3.
 */

/**
 * @defgroup app_specific_defines Application-specific macro definitions
 * @brief Application-specific macro definitions
 */

#include <stdint.h>
#include <string.h>

#include "blesc_error.h"
#include "app_config.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "global_app_config.h"

/* Tasks */
#include "task_bleam.h"
#include "task_board.h"
#include "task_config.h"
#include "task_connect_common.h"
#include "task_fds.h"
#include "task_scan_connect.h"
#include "task_scan.h"
#include "task_signature.h"
#include "task_storage.h"
#include "task_time.h"

/* BLE */
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_conn_state.h"
#include "ble_db_discovery.h"
#include "bleam_discovery.h"
#include "ble_hci.h"
#include "bleam_service.h"
#include "config_service.h"
#include "bleam_send_helper.h"

/* Button module */
#include "bsp.h"

/* Core */
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_delay.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_power.h"
#if defined(SDK_15_3)
  #include "nrf_sdh.h"
  #include "nrf_sdh_ble.h"
  #include "nrf_sdh_soc.h"
#endif
#if defined(SDK_12_3)
  #include "softdevice_handler.h"
#endif

#ifdef BLESC_DFU
/* DFU */
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"
  #if defined(SDK_15_3)
    #include "nrf_dfu_ble_svci_bond_sharing.h"
    #include "peer_manager.h"
    #include "peer_manager_handler.h"
  #endif
  #if defined(SDK_12_3)
    #include "nrf_dfu_settings.h"
  #endif
#endif

/* Timer */
#include "nrf_drv_clock.h"

/* Crypto */
#if defined(SDK_15_3)
#include "nrf_crypto_init.h"
#include "nrf_crypto_rng.h"
#endif
#if defined(SDK_12_3)
#include "nrf_crypto.h"
#endif

/* Ruuvi drivers */
#ifdef BOARD_RUUVITAG_B
#include "ruuvi_boards.h"
#include "ruuvi_application_config.h"
#endif

/* Logging */
#include "log.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#if defined(SDK_15_3)
  #include "nrf_log_default_backends.h"
#endif

/**@addtogruop blesc_config
 * @{ */
/* GAP init macros */
#define DEVICE_NAME                    APP_CONFIG_DEVICE_NAME           /**< Name of device. Will be included in the advertising data. */
#define MIN_CONN_INTERVAL              MSEC_TO_UNITS(15, UNIT_1_25_MS)  /**< Minimum acceptable connection interval. */
#define MAX_CONN_INTERVAL              MSEC_TO_UNITS(15, UNIT_1_25_MS)  /**< Maximum acceptable connection interval. */
#define SLAVE_LATENCY                  29                               /**< Slave latency. */
#define CONN_SUP_TIMEOUT               MSEC_TO_UNITS(5600, UNIT_10_MS)  /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
/** @} end of blesc_config */

/** @addtogroup bleam_connect
 * @{ */
/* Conn init macros */
#define FIRST_CONN_PARAMS_UPDATE_DELAY __TIMER_TICKS(5000)              /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY  __TIMER_TICKS(30000)             /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT   3                                /**< Number of attempts before giving up the connection parameter negotiation. */
/** @} end of bleam_connect */

/** @addtogroup blesc_app
 * @{ */

/* Misc */
#define DEAD_BEEF                      0xDEADBEEF                       /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define APP_TIMER_OP_QUEUE_SIZE        10                               /**< Size of timer operation queues. */


/************************* DECLARATIONS ***************************/

#if defined(SDK_15_3)
NRF_BLE_GATT_DEF(m_gatt);                         /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                           /**< Context for the Queued Write module.*/
#endif
#if defined(SDK_12_3)
static nrf_ble_gatt_t m_gatt;
static nrf_ble_qwr_t m_qwr;
#endif
BLEAM_SERVICE_DISCOVERY_DEF(m_db_disc);           /**< Bleam discovery module instance. */
CONFIG_S_SERVER_DEF(m_config_s_server);           /**< Configuration service server instance. */
BLEAM_SERVICE_CLIENT_DEF(m_bleam_service_client); /**< Bleam service client instance. */
NRF_BLE_SCAN_DEF(m_scan);                         /**< Scanning module instance. */

APP_TIMER_DEF(drop_blacklist_timer_id);    /**< @ingroup ios_solution
                                             *  Timer for cleaning iOS blacklist. */

extern uint16_t m_conn_handle; /**< Handle of the current connection. */

/**********************  INTERNAL FUNCTIONS  ************************/

/** @addtogroup blesc_dfu
 * @{ */

#ifdef BLESC_DFU
#if defined(SDK_15_3)
/**@brief Handler for shutdown preparation.
 *
 * @details During shutdown procedures, this function will be called at a 1 second interval
 *          untill the function returns true. When the function returns true, it means that the
 *          app is ready to reset to DFU mode.
 *
 * @param[in]   event   Power manager event.
 *
 * @retval true if shutdown is allowed by this power manager handler.
 * @retval false otherwise.
 *
 * @note So far, always returns true because there are yet no conditions for denying resetting to DFU mode.
 */
static bool app_shutdown_handler(nrf_pwr_mgmt_evt_t event) {
    switch (event) {
    case NRF_PWR_MGMT_EVT_PREPARE_DFU:
        NRF_LOG_INFO("Power management wants to reset to DFU mode.");
        break;

    default:
        return true;
    }

    NRF_LOG_INFO("Power management allowed to reset to DFU mode.");
    return true;
}

/**@brief Register application shutdown handler with priority 0.
 */
NRF_PWR_MGMT_HANDLER_REGISTER(app_shutdown_handler, 0);
#endif
#if defined(SDK_12_3)
/** @brief Function for checking if DFU mode should be entered.
 *
 * @note This function is a copy of the same function from nRF DFU bootloader module.
 *
 * @retval true  if DFU mode must be entered.
 * @retval false if there is no need to enter DFU mode.
 */
bool nrf_dfu_enter_check(void) {
    if (NRF_POWER->GPREGRET == BOOTLOADER_DFU_START) {
        NRF_POWER->GPREGRET = 0;
        return true;
    }

    if (s_dfu_settings.enter_buttonless_dfu == 1) {
        s_dfu_settings.enter_buttonless_dfu = 0;
        APP_ERROR_CHECK(nrf_dfu_settings_write(NULL));
        return true;
    }
    return false;
}

NRF_PWR_MGMT_REGISTER_HANDLER(app_shutdown_handler);
#endif

/**@brief Function for booting the bootloader.
 *
 * @retval NRF_SUCCESS in case of successful shutdown.
 */
ret_code_t enter_dfu_mode(void) {
    blesc_toggle_leds(0, 0);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "====== Entering DFU ======\r\n");
    ret_code_t err_code;
#if defined(SDK_15_3)
    err_code = sd_power_gpregret_clr(0, 0xffffffff);
    VERIFY_SUCCESS(err_code);

    err_code = sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
    VERIFY_SUCCESS(err_code);
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);
#endif
#if defined(SDK_12_3)
    err_code = sd_power_gpregret_clr(0xffffffff);
    VERIFY_SUCCESS(err_code);

    err_code = sd_power_gpregret_set(BOOTLOADER_DFU_START);
    VERIFY_SUCCESS(err_code);
    wdt_feed();
    sd_nvic_SystemReset();
#endif
    // Signal that DFU mode is to be enter to the power management module
    return NRF_SUCCESS;
}
#endif
/** @} end of blesc_dfu*/

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/***********************  HANDLERS  *************************/

/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 *
 * @returns Nothing.
 */
static void idle_state_handle(void) {
    UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
    nrf_pwr_mgmt_run();
    wdt_feed();
}

/**@brief Function for handling BLE events.
 * @ingroup bleam_connect
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 *
 * @returns Nothing.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {
    uint32_t err_code;
    ble_gap_evt_t const *p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Gap event: Connected\r\n");

        if(BLE_CONN_HANDLE_INVALID == p_gap_evt->conn_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Connection handle is bogus\r\n");
            err_code = sd_ble_gap_disconnect(p_gap_evt->conn_handle,
                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if(NRF_ERROR_INVALID_STATE != err_code)
                APP_ERROR_CHECK(err_code);
        }

        // Connected to config service
        if(CONFIG_S_STATUS_WAITING == config_s_get_status()) {
            handle_connect_config(p_ble_evt, &m_qwr);
        } else
        // If unknown iOS, we look for Bleam service the stupid way via service discovery and GATTC read
        if (CONFIG_S_STATUS_DONE == config_s_get_status() && stupid_ios_data_active()) {
            handle_connect_ios(p_ble_evt);
        } else
        // Connected to Bleam and configuration is over
        if (CONFIG_S_STATUS_DONE == config_s_get_status()) {
            handle_connect_bleam(p_ble_evt, &m_qwr);
        } else {
            __LOG(LOG_SRC_APP, LOG_LEVEL_WARN, "Connection shouldn't happen\r\n");
            err_code = sd_ble_gap_disconnect(p_gap_evt->conn_handle,
                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if(NRF_ERROR_INVALID_STATE != err_code)
                APP_ERROR_CHECK(err_code);
        }
        break;

    case BLE_GAP_EVT_DISCONNECTED:
    case BLE_GAP_EVT_TIMEOUT:
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Gap event: Disconnected or timed out\r\n");
        m_conn_handle = BLE_CONN_HANDLE_INVALID;
        if(CONFIG_S_STATUS_DONE == config_s_get_status()) {
            handle_disconnect();
        }
        break;


#if defined(SDK_15_3)
    case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "PHY update request.\r\n");
        ble_gap_phys_t const phys = {
            .rx_phys = BLE_GAP_PHY_AUTO,
            .tx_phys = BLE_GAP_PHY_AUTO,
        };
        err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
        APP_ERROR_CHECK(err_code);
        break;
#endif

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        // Pairing not supported
        err_code = sd_ble_gap_sec_params_reply(p_ble_evt->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
        APP_ERROR_CHECK(err_code);
        break;

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        // No system attributes have been stored.
        err_code = sd_ble_gatts_sys_attr_set(p_ble_evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
        APP_ERROR_CHECK(err_code);
        break;

    case BLE_GATTC_EVT_TIMEOUT:
        // Disconnect on GATT Client timeout event.
        if(BLE_CONN_HANDLE_INVALID != m_conn_handle && p_ble_evt->evt.gattc_evt.conn_handle == m_conn_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "GATT Client Timeout.\r\n");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if(NRF_ERROR_INVALID_STATE != err_code)
                APP_ERROR_CHECK(err_code);
        }
        break;

    case BLE_GATTS_EVT_TIMEOUT:
        // Disconnect on GATT Server timeout event.
        if(BLE_CONN_HANDLE_INVALID != m_conn_handle && p_ble_evt->evt.gatts_evt.conn_handle == m_conn_handle) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2,  "GATT Server Timeout.\r\n");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if(NRF_ERROR_INVALID_STATE != err_code)
                APP_ERROR_CHECK(err_code);
        }
        break;

    default:
        // No implementation needed.
        break;
    }
}

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 *
 * @returns Nothing.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error) {
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling an event from the Connection Parameters Module.
 * @ingroup bleam_connect
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 *
 * @returns Nothing.
 */
static void conn_params_evt_handler(ble_conn_params_evt_t *p_evt) {
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Conn params event FAILED\r\n");
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        if(NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling errors from the Connection Parameters module.
 * @ingroup bleam_connect
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 *
 * @returns Nothing.
 */
static void conn_params_error_handler(uint32_t nrf_error) {
    if(NRF_ERROR_INVALID_STATE != nrf_error)
        APP_ERROR_HANDLER(nrf_error);
}

#ifdef SDK_12_3
/**@addtogroup nrf51_handlers
 * @{
 */

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack event has
 * been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 *
 * @returns Nothing.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt) {
    bleam_service_client_on_ble_evt    (p_ble_evt,  &m_bleam_service_client);
    config_s_server_on_ble_evt         (p_ble_evt,  &m_config_s_server);
    ble_db_discovery_on_ble_evt        (&m_db_disc, p_ble_evt);
    bleam_service_discovery_on_ble_evt (p_ble_evt,  &m_db_disc);
    nrf_ble_scan_on_ble_evt            (p_ble_evt,  &m_scan);
    ble_evt_handler                    (p_ble_evt,  NULL);
    ble_conn_params_on_ble_evt         (p_ble_evt);
}

/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 *
 * @returns Nothing.
 */
static void sys_evt_dispatch(uint32_t sys_evt) {
    // Dispatch the system event to the fstorage module, where it will be
    // dispatched to the Flash Data Storage (FDS) module.
    fs_sys_event_handler(sys_evt);
}
/** @} end of nrf51_handlers */
#endif

/*************************  INITIALIZERS ***************************/

/**@brief Function for initializing the logging module, both @link_lib_nrf_log and nRF Mesh SDK logging.
 * @ingroup blesc_debug
 *
 * @returns Nothing.
 */
static void logging_init(void) {
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

#if defined(SDK_15_3)
    NRF_LOG_DEFAULT_BACKENDS_INIT();
#endif
    __LOG_INIT(LOG_SRC_APP | LOG_SRC_FRIEND, APP_CONFIG_LOG_LEVEL, LOG_CALLBACK_DEFAULT);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "====== Booting ======\n\n\nLogging initialised.\r\n");
}

/**@brief Function for initializing the timer module.
 * @ingroup bleam_time
 *
 * @details Besides initialising all main application timers, this function also
 *          sets initial system time values until the time is updated via Bleam connection.
 *
 * @returns Nothing.
 */
static void timers_init(void) {
    ret_code_t err_code = NRF_SUCCESS;
#if defined(SDK_15_3)
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
#endif
#if defined(SDK_12_3)
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
#endif

    system_time_init();
 
    // Timer for blacklist
    err_code = app_timer_create(&drop_blacklist_timer_id, APP_TIMER_MODE_REPEATED, drop_blacklist);
    APP_ERROR_CHECK(err_code);

    // Start watchdog timer
    app_timer_start(drop_blacklist_timer_id, MACLIST_TIMEOUT, NULL);
}

#ifdef BLESC_DFU
/**@brief Function for initializing RuuviTag DFU.
 * @ingroup blesc_dfu
 *
 * @returns Nothing.
 */
static void dfu_init (void) {
  #if defined(SDK_15_3)
    // Initialize the async SVCI interface to bootloader before any interrupts are enabled.
    ret_code_t err_code = ble_dfu_buttonless_async_svci_init();
    APP_ERROR_CHECK(err_code);
  #endif
  #if defined(SDK_12_3)
    #ifndef BOARD_IBKS_PLUS
    nrf_dfu_settings_init();
    #endif
  #endif
}
#endif

/**@brief Function for initializing power management.
 *
 * @returns Nothing.
 */
static void power_management_init(void) {
    ret_code_t err_code;
#if defined(SDK_15_3)
    err_code = nrf_pwr_mgmt_init();
#endif
#if defined(SDK_12_3)
    err_code = nrf_pwr_mgmt_init(__TIMER_TICKS(1000));
#endif
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the database discovery module.
 * @ingroup bleam_connect
 *
 * @returns Nothing.
 */
static void db_discovery_init(void)
{
    ret_code_t err_code;
    err_code = bleam_service_discovery_init(&bleam_service_discovery_evt_handler, BLEAM_SERVICE_UUID);
    APP_ERROR_CHECK(err_code);
    err_code = ble_db_discovery_init(db_disc_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 *
 * @returns Nothing.
 */
static void ble_stack_init(void) {
    ret_code_t err_code;
#if defined(SDK_15_3)
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
#endif
#if defined(SDK_12_3)
    // If SD is enabled, disable it first
    if(softdevice_handler_is_enabled()) {
        err_code = softdevice_handler_sd_disable();
        APP_ERROR_CHECK(err_code);
    }

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(NRF_SDH_BLE_CENTRAL_LINK_COUNT,
                                                    NRF_SDH_BLE_PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Check the RAM settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
#if (NRF_SD_BLE_API_VERSION == 3)
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
#endif

    // Wait for random bytes
    size_t cnt = 0;
    for (; 100 > cnt; ++cnt) {
        uint8_t bytes;
        sd_rand_application_bytes_available_get(&bytes);
        if(2 > bytes) {
            nrf_delay_ms(100);
        } else
            break;
    }
    if(100 <= cnt) {
        APP_ERROR_CHECK(NRF_ERROR_NO_MEM);
    }
}

/**@brief Function for the GAP initialization.
 * @ingroup blesc_config
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 *
 * @returns Nothing.
 */
static void gap_params_init(void) {
    uint32_t err_code;
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
        (const uint8_t *)DEVICE_NAME,
        strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the GATT library.
 * @ingroup bleam_connect
 *
 * @returns Nothing.
 */
static void gatt_init(void) {
#if defined(SDK_15_3)
    ret_code_t err_code;
    err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, BLE_GATT_ATT_MTU_DEFAULT);
    APP_ERROR_CHECK(err_code);
#endif
}

/**@brief Function for initializing basic services that will be used in all Bleam Scanner modes.
 *
 * @returns Nothing.
 */
static void basic_services_init(void) {
    uint32_t err_code;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

#if defined(SDK_15_3)
    // Crypto init
    err_code = nrf_crypto_init();
    APP_ERROR_CHECK(err_code);

    // Random number generation init
    err_code = nrf_crypto_rng_init(NULL, NULL);
    APP_ERROR_CHECK(err_code);
#endif
#if defined(SDK_12_3)
    // Crypto init
    ecc_init(true);

    // Random number generation init
    nrf_drv_rng_config_t rng_init = NRF_DRV_RNG_DEFAULT_CONFIG;
    err_code = nrf_drv_rng_init(&rng_init);
    APP_ERROR_CHECK(err_code);
#endif
}

/**@brief Function for initializing the Connection Parameters module.
 * @ingroup bleam_connect
 *
 * @returns Nothing.
 */
static void conn_params_init(void) {
    uint32_t err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = conn_params_evt_handler;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

/***********************  MAIN  **********************/

/**@brief Second batch of initializers after the Bleam Scanner mode is determined.
 *
 * Bleam Scanner is started up in either Configuration mode or Scanning mode.
 *
 * @returns Nothing.
 */
static void init_finalize(void) {
    if(BLESC_STATE_INIT == blesc_node_state_get())
        return;
    blesc_node_state_set(BLESC_STATE_INIT);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Looking for configuration record...\r\n");

    // Check if node has config data saved in flash
    ret_code_t err_code = flash_config_load();
    if (NRF_SUCCESS == err_code) {
        flash_params_load();
        blesc_services_init(&m_bleam_service_client, ble_stack_init);
        config_s_finish();
        conn_params_init();
        scan_connect_init(&m_db_disc, &m_scan);

        blesc_keys_t *keys = blesc_keys_get();
        create_blesc_public_key(keys);

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam Scanner is starting with Node ID %04X.\r\n", blesc_node_id_get());
        scan_start();
    } else { // if (NRF_ERROR_NOT_FOUND == err_code)
        config_mode_services_init(&m_config_s_server);
#ifndef HARDCODED_CONFIG
        conn_params_init();
        advertising_init();
#endif
    }
}

/**@brief First batch of initializers before @ref flash_init call.
 *
 * @returns Nothing.
 */
static void init_start(void) {
    logging_init();
    ble_stack_init();
    blesc_error_on_boot();

    timers_init();
    
#if NRF_MODULE_ENABLED(DEBUG)
    bool erase_bonds;
    buttons_init(&erase_bonds);
    leds_init();
#endif
    adc_init();

#ifdef BLESC_DFU
    dfu_init();
#endif
    wdt_init();
    power_management_init();
    db_discovery_init();
    basic_services_init();
    gap_params_init();
    gatt_init();
    connect_common_init();
    flash_init(init_finalize);
    // All following inits are called after INIT event is caught by @ref fds_evt_handler
}

/**@brief Application main function.
 */
int main(void) {
    init_start();

    // Enter main loop.
    for (;;) {
        idle_state_handle();
    }
}
/** @} end of blesc_app */

/**
 * @}
 */