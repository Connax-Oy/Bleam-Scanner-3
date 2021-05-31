/**
 * @addtogroup task_board
 * @{
 */

/**
 * @defgroup watchdog Watchdog
 * @brief Watchdog functions
 */
 
/**
 * @defgroup battery Battery
 * @brief Measuring battery level with ADC sensor
 */
 
/**
 * @defgroup leds_and_buttons LEDs and buttons
 * @brief LEDs and buttons handlers
 */

 #ifndef BLESC_BOARD_SUPPORT_H__
#define BLESC_BOARD_SUPPORT_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

#include "bsp.h"
#if defined(SDK_15_3)
  #include "nrf_drv_saadc.h"
#endif
#if defined(SDK_12_3)
  #include "nrf_drv_adc.h"
#endif

/**
 * @addtogroup battery
 * @{
 */

/* Battery level measurement macros */
#define ADC_REF_VOLTAGE_IN_MILLIVOLTS  1200 /**< Reference voltage (in milli volts) used by ADC while doing conversion. */
#define ADC_PRE_SCALING_COMPENSATION   3    /**< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.*/
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS 270  /**< Typical forward voltage drop of the diode . */
#define ADC_RES_10BIT                  1024 /**< Maximum digital value for 10-bit ADC conversion. */

/**@brief Macro to convert the result of ADC conversion in millivolts.
 *
 * @param[in]  ADC_VALUE   ADC result.
 *
 * @returns Result converted to millivolts.
 */
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)\
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION)

/** @} end of battery */

/**
 * @addtogroup watchdog
 * @{
 */

/**@brief Function for initializing the watchdog.
 */
void wdt_init(void);

/**@brief Function for feeding the watchdog.
 */
void wdt_feed(void);

/** @} end of watchdog */

/**
 * @addtogroup leds_and_buttons
 * @{
 */

#if NRF_MODULE_ENABLED(DEBUG)
/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
void leds_init(void);
#endif

/**@brief Fuction for Bleam Scanner LED indication.
 *
 * @param[in]  scanning_led_state   Desired state for the scanning LED.
 * @param[in]  connected_led_state  Desired state for the connected LED.
 *
 * @returns Nothing.
 */
void blesc_toggle_leds(bool scanning_led_state, bool connected_led_state);

#if NRF_MODULE_ENABLED(DEBUG)
/**@brief Function for initializing buttons.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 *
 * @returns Nothing.
 */
void buttons_init(bool *p_erase_bonds);

/**@brief Common handler for button events.
 *
 * @returns Nothing.
 */
void button_event_handler(void);
#endif

/** @} end of leds_and_buttons */

/**
 * @addtogroup battery
 * @{
 */

/**@brief Function for configuring nRF ADC to do battery level conversion.
 *
 * @returns Nothing.
 */
void adc_init(void);

/**@brief Function for initiating health data send
 *
 * @param[in]  voltage_batt_lvl   Battery level in volts.
 *
 * @details This function will initiate putting voltage and other health data to queue.
 *
 * @returns Nothing.
 */
void battery_level_send(float voltage_batt_lvl);

/**@brief Function for battery measurement
 *
 * @details This function will start the ADC/SAADC.
 *
 * @returns Nothing.
 */
void battery_level_measure(void);

#if defined(SDK_15_3) && defined(BOARD_DK)
/**@brief Function for handling the SAADC interrupt.
 *
 * @details  This function will fetch the conversion result from the SAADC, convert the value into
 *           percentage and send it to peer.
 *
 * @returns Nothing.
 */
void saadc_event_handler(nrf_drv_saadc_evt_t const * p_event);
#endif
#if defined(SDK_12_3)
/**@brief Function for handling the ADC interrupt.
 *
 * @details  This function will fetch the conversion result from the ADC, convert the value into
 *           percentage and send it to peer.
 *
 * @returns Nothing.
 */
void adc_event_handler(nrf_drv_adc_evt_t const * p_event);
#endif
/** @} end of battery */

#endif // BLESC_BOARD_SUPPORT_H__

/** @}*/