#ifndef APP_CONFIG_PLATFORM_H__
#define APP_CONFIG_PLATFORM_H__

#include <stdbool.h>

/**
 * @defgroup NRF51_SD_SPECIFIC_DEFINES_AND_INCLUDES nRF51 headers and defines
 * @{
 * @ingroup app_specific_defines
 */

#include "ble.h"
#include "ble_err.h"
#include "ble_gap.h"
#include "ble_gatt.h"
#include "ble_gattc.h"
#include "ble_gatts.h"
#include "ble_hci.h"
#include "ble_l2cap.h"
#include "nrf_error.h"
#include "nrf_error_sdm.h"
#include "nrf_error_soc.h"
#include "nrf_nvic.h"
#include "nrf_sd_def.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"
#include "nrf_svc.h"

#include "nrf51_bootloader_gpregret_values.h"

#define APP_TIMER_PRESCALER  0   /**< Value of the RTC1 PRESCALER register. */

/**@brief Macro function for wrapping nRF51's @link_12_app_timer_ticks macro function
 *        into the same interface across platforms.
 * @param[in] _interval    Time interval in seconds
 *
 * @returns Corresponding time interval in ticks
 */
#define __TIMER_TICKS(_interval) APP_TIMER_TICKS(_interval, APP_TIMER_PRESCALER)

/** @} end of NRF51_SD_SPECIFIC_DEFINES_AND_INCLUDES */

#define NRF_PWR_MGMT_ENABLED 1
#define NRF_DFU_TRANSPORT_BLE 1

/** Override default sdk_config.h values. */

#define NRF_FPRINTF_ENABLED 1
#define NRF_FPRINTF_FLAG_AUTOMATIC_CR_ON_LF_ENABLED 0
#define NRF_LOG_BACKEND_SERIAL_USES_RTT 1
#define NRF_LOG_ENABLED 0
#define NRF_STRERROR_ENABLED 1

#define APP_SCHEDULER_ENABLED 1
#define CRC32_ENABLED 1
#define FDS_VIRTUAL_PAGES_RESERVED 0

#define ADC_ENABLED 1

#ifndef BLE_GAP_ADV_SET_DATA_SIZE_MAX
#define BLE_GAP_ADV_SET_DATA_SIZE_MAX (31)   /**< Maximum data length for an advertising set. */
#endif

#endif /* APP_CONFIG_PLATFORM_H__ */