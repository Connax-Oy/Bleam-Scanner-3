/** @file task_board.c
 *
 * @defgroup task_board Task Board
 * @{
 * @ingroup blesc_debug
 * @ingroup blesc_tasks
 *
 * @brief Handles LEDs, buttons, ADC sensor and watchdog.
 */
#include "task_board.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#if defined(SDK_15_3)
  #include "nrf_log_default_backends.h"
#endif
#include "nrf_drv_wdt.h"

#include "bleam_send_helper.h"

#include "task_fds.h"
#include "task_config.h"
#include "task_time.h"

#ifdef BLESC_DFU
ret_code_t enter_dfu_mode(void); // Forward declaration of a function from main.c !!! might not work
#endif

/**
 * @addtogroup watchdog
 * @{
 */

nrf_drv_wdt_channel_id m_channel_id; /**< Watchdog channel */

/**@brief WDT events handler.
 *
 * @returns Nothing.
 */
static void wdt_event_handler(void)
{
    //NOTE: The max amount of time we can spend in WDT interrupt is two cycles of 32768[Hz] clock - after that, reset occurs
    blesc_toggle_leds(0, 0);
}

/**@brief Function for initializing the watchdog.
 *
 * @returns Nothing.
 */
void wdt_init(void) {
    ret_code_t err_code = NRF_SUCCESS;
    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_wdt_channel_alloc(&m_channel_id);
    APP_ERROR_CHECK(err_code);
    nrf_drv_wdt_enable();
}

/**@brief Function for feeding the watchdog channel.
 *
 * @returns Nothing.
 */
void wdt_feed(void) {
    nrf_drv_wdt_channel_feed(m_channel_id);
}

/** @} end of watchdog */

void button_event_handler(void) {
    if (0 == blesc_node_id_get()) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Device isn't configured.\r\n");
  #if defined(BLESC_DFU)
        enter_dfu_mode();
  #else // !defined(BLESC_DFU)
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "This firmware does not support DFU.\r\n");
  #endif
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "====== Node unconfiguration ======\n");
        flash_config_delete();
    }
}

void battery_level_send(float voltage_batt_lvl) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Battery level is at " NRF_LOG_FLOAT_MARKER " V, %d.\r\n", NRF_LOG_FLOAT(voltage_batt_lvl), (int)(voltage_batt_lvl * 10));
    bleam_health_queue_add(voltage_batt_lvl * 10, get_blesc_uptime(), get_system_time(), get_sleep_time_sum());
}

/** @}*/