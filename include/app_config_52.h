#ifndef APP_CONFIG_PLATFORM_H__
#define APP_CONFIG_PLATFORM_H__

#include <stdbool.h>

/**
 * @defgroup NRF52_SD_SPECIFIC_DEFINES_AND_INCLUDES nRF52 headers and defines
 * @{
 * @ingroup app_specific_defines
 */

/**@brief Macro function for wrapping nRF52's @link_app_timer_ticks macro function
 *        into the same interface across platforms.
 * @param[in] _interval    Time interval in seconds
 *
 * @returns Corresponding time interval in ticks
 */
#define __TIMER_TICKS(_interval) APP_TIMER_TICKS(_interval)

/** @} end of NRF52_SD_SPECIFIC_DEFINES_AND_INCLUDES */

/* BLE stack init macros */
#define APP_BLE_CONN_CFG_TAG           1                                                  /**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO          3                                                  /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#ifdef BLESC_DFU
  #define NRF_DFU_BLE_BUTTONLESS_SUPPORTS_BONDS 0
#endif
  #define NRF_DFU_TRANSPORT_BLE 1

/* Override default sdk_config.h values. */

#ifdef BOARD_RUUVITAG_B
#include "application_driver_configuration.h"
#else
#define NRF_LOG_ENABLED 1
#endif

#define SAADC_ENABLED 1

#endif /* APP_CONFIG_PLATFORM_H__ */