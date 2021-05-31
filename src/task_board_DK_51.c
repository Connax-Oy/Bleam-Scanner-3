/** @file task_board_DK_51.c
 *
 * @defgroup task_board_DK_51 nRF51 board definitions and functions
 * @{
 * @ingroup task_board
 *
 * @brief Handles LEDs, buttons, ADC sensor and watchdog for nRF51822 board.
 */
#include "task_board.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#include "task_fds.h"
#include "task_config.h"
#include "task_time.h"

#define CENTRAL_SCANNING_LED         BSP_BOARD_LED_0        /**< Scanning LED will be on when the device is scanning. */
#define CENTRAL_CONNECTED_LED        BSP_BOARD_LED_1        /**< Connected LED will be on when the device is connected. */

#define ADC_BUFFER_SIZE 1                        /**< Size of buffer for ADC samples.  */
static nrf_adc_value_t adc_buf[ADC_BUFFER_SIZE]; /**< ADC buffer. */

/*************** LEDs ****************/

#if NRF_MODULE_ENABLED(DEBUG)
void leds_init(void) {
    bsp_board_leds_init();
    bsp_board_led_on(CENTRAL_CONNECTED_LED);
    bsp_board_led_on(CENTRAL_SCANNING_LED);
}
#endif // NRF_MODULE_ENABLED(DEBUG)

void blesc_toggle_leds(bool scanning_led_state, bool connected_led_state) {
#if NRF_MODULE_ENABLED(DEBUG)
    if(scanning_led_state)
        bsp_board_led_on(CENTRAL_SCANNING_LED);
    else
        bsp_board_led_off(CENTRAL_SCANNING_LED);
    if(connected_led_state)
        bsp_board_led_on(CENTRAL_CONNECTED_LED);
    else
        bsp_board_led_off(CENTRAL_CONNECTED_LED);
#endif // NRF_MODULE_ENABLED(DEBUG)
}


/*************** Buttons ****************/

#if NRF_MODULE_ENABLED(DEBUG)
/**@brief Handler for BSP button events.
 * @ingroup leds_and_buttons
 *
 * @param[in]  event    Pointer to the BSP event to handle.
 */
static void bsp_event_handler(bsp_event_t event) {
    switch (event)
    {
        case BSP_EVENT_KEY_0:
        case BSP_EVENT_KEY_1:
        case BSP_EVENT_KEY_2:
        case BSP_EVENT_KEY_3:
            button_event_handler();
            break;

        default:
            break;
    }
}

void buttons_init(bool *p_erase_bonds) {
    ret_code_t err_code;
    err_code = bsp_init(BSP_INIT_BUTTONS, __TIMER_TICKS(100), bsp_event_handler);
    APP_ERROR_CHECK(err_code);
//    bsp_board_buttons_init();
}
#endif // NRF_MODULE_ENABLED(DEBUG)


/*************** ADC ****************/

void adc_init(void) {
    ret_code_t err_code = NRF_SUCCESS;
    nrf_drv_adc_config_t config = NRF_DRV_ADC_DEFAULT_CONFIG;
    err_code = nrf_drv_adc_init(&config, adc_event_handler);
    APP_ERROR_CHECK(err_code);

    static nrf_drv_adc_channel_t adc_channel = NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_2);
    nrf_drv_adc_channel_enable(&adc_channel);

    err_code = nrf_drv_adc_buffer_convert(adc_buf, ADC_BUFFER_SIZE);
    APP_ERROR_CHECK(err_code);
}

void battery_level_measure(void) {
    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Battery level measurement request.\r\n");
    nrf_drv_adc_sample();
}

void adc_event_handler(nrf_drv_adc_evt_t const * p_event) {
    if (p_event->type == NRF_DRV_ADC_EVT_DONE) {
        nrf_adc_value_t adc_result;
        float           voltage_batt_lvl;
        uint8_t         percentage_batt_lvl;
        uint32_t        err_code;

        adc_result = p_event->data.done.p_buffer[0];
        err_code = nrf_drv_adc_buffer_convert(p_event->data.done.p_buffer, 1);
        APP_ERROR_CHECK(err_code);

        voltage_batt_lvl = (ADC_RESULT_IN_MILLI_VOLTS(adc_result) + DIODE_FWD_VOLT_DROP_MILLIVOLTS) * 0.001;

        battery_level_send(voltage_batt_lvl);
    }
}

/** @} */