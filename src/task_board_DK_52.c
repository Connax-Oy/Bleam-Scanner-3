/** @file task_board_DK_52.c
 *
 * @defgroup task_board_DK_52 nRF52 board definitions and functions
 * @{
 * @ingroup task_board
 *
 * @brief Handles LEDs, buttons, ADC sensor and watchdog for nRF52832 and nRF52840 boards.
 */
#include "task_board.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "task_fds.h"
#include "task_config.h"
#include "task_time.h"

#define CENTRAL_SCANNING_LED         BSP_BOARD_LED_0        /**< Scanning LED will be on when the device is scanning. */
#define CENTRAL_CONNECTED_LED        BSP_BOARD_LED_1        /**< Connected LED will be on when the device is connected. */

static nrf_saadc_value_t adc_buf[2]; /**< ADC buffer. */

/*************** LEDs ****************/

#if NRF_MODULE_ENABLED(DEBUG)
void leds_init(void) {
    bsp_board_init(BSP_INIT_LEDS);
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
    bsp_event_t startup_event;

    err_code = bsp_init(BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}
#endif // NRF_MODULE_ENABLED(DEBUG)


/*************** ADC ****************/

void adc_init(void) {
    ret_code_t err_code = NRF_SUCCESS;
    err_code = nrf_drv_saadc_init(NULL, saadc_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_saadc_channel_config_t config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_VDD);
    err_code = nrf_drv_saadc_channel_init(0, &config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(&adc_buf[0], 1);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(&adc_buf[1], 1);
    APP_ERROR_CHECK(err_code);
}

void battery_level_measure(void) {
    ret_code_t err_code;
    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Battery level measurement request.\r\n");
    err_code = nrf_drv_saadc_sample();
    APP_ERROR_CHECK(err_code);
}

void saadc_event_handler(nrf_drv_saadc_evt_t const * p_event) {
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {
        nrf_saadc_value_t adc_result;
        float             voltage_batt_lvl;
        uint8_t           percentage_batt_lvl;
        uint32_t          err_code;

        adc_result = p_event->data.done.p_buffer[0];
        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
        APP_ERROR_CHECK(err_code);

        voltage_batt_lvl = (ADC_RESULT_IN_MILLI_VOLTS(adc_result) + DIODE_FWD_VOLT_DROP_MILLIVOLTS) * 0.001;

        battery_level_send(voltage_batt_lvl);
    }
}

/** @} */