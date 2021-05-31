#ifndef APP_CONFIG_H__
#define APP_CONFIG_H__

#include <stdbool.h>

#include "blesc_log_settings.h"

/**
 * @addtogroup app_specific_defines
 *
 * @{
 */
 
#if defined(SDK_15_3)
  #include "app_config_52.h"
#endif
#if defined(SDK_12_3)
  #include "app_config_51.h"
#endif


/** Macro for app error handler
 * @ingroup blesc_debug
 */
#define DEBUG 1

/** Value to be retained in GPREGRET to differentiate between soft and hard resets. */
#define BLESC_GPREGRET_RETAINED_VALUE  0x0E

/** @} end of app_specific_defines */

#ifdef BLESC_DFU
  #define NRF_DFU_BLE_BUTTONLESS_SUPPORTS_BONDS 0
#endif
  #define NRF_DFU_TRANSPORT_BLE 1

/* Override default sdk_config.h values. */

#define WDT_ENABLED 1

#define FDS_ENABLED 1
#define NRF_FSTORAGE_ENABLED FDS_ENABLED
#define FDS_VIRTUAL_PAGES 6
#define NRF_DFU_APP_DATA_AREA_SIZE (CODE_PAGE_SIZE * FDS_VIRTUAL_PAGES)

/* Configuration for the BLE SoftDevice support module to be enabled. */

#define NRF_SDH_ENABLED 1
#define NRF_SDH_BLE_ENABLED 1
#define NRF_SDH_SOC_ENABLED 1
#define NRF_BLE_CONN_PARAMS_ENABLED 1
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 23
#define NRF_SDH_BLE_GAP_DATA_LENGTH 27
#define NRF_SDH_BLE_PERIPHERAL_LINK_COUNT 1
#define NRF_SDH_BLE_SERVICE_CHANGED 1
#define NRF_QUEUE_ENABLED 1

#define NRF_CRYPTO_ENABLED 1
#define NRF_CRYPTO_HMAC_ENABLED 1
#define NRF_CRYPTO_BACKEND_OBERON_ENABLED 1
#define NRF_CRYPTO_BACKEND_NRF_HW_RNG_ENABLED 1
#define RNG_ENABLED 1
#define NRF_CRYPTO_RNG_STATIC_MEMORY_BUFFERS_ENABLED 1
#define NRF_CRYPTO_RNG_AUTO_INIT_ENABLED 0

#define APP_TIMER_ENABLED 1
#define APP_TIMER_KEEPS_RTC_ACTIVE 1

#ifdef APP_TIMER_CONFIG_RTC_FREQUENCY
#undef APP_TIMER_CONFIG_RTC_FREQUENCY
#endif
#define APP_TIMER_CONFIG_RTC_FREQUENCY 0

#endif /* APP_CONFIG_H__ */