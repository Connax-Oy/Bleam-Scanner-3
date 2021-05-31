/** @file task_board_ruuvi.c
 *
 * @defgroup task_board_ruuvi RuuviTag board definitions and functions
 * @{
 * @ingroup task_board
 *
 * @brief Handles LEDs, buttons, ADC sensor and watchdog for RuuviTag 52832 board.
 */
#include "task_board.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ruuvi_boards.h"
#include "ruuvi_driver_error.h"
#include "ruuvi_driver_sensor.h"
#include "ruuvi_interface_gpio.h"
#include "ruuvi_interface_adc.h"
#include "ruuvi_interface_adc_mcu.h"
#include "ruuvi_interface_log.h"
#include "ruuvi_application_config.h"

#include "ruuvi_interface_yield.h"
#include "ruuvi_interface_gpio.h"
#include "ruuvi_interface_gpio_interrupt.h"
#include "ruuvi_interface_acceleration.h"

#include "task_fds.h"
#include "task_config.h"
#include "task_time.h"

#define CENTRAL_SCANNING_LED         RUUVI_BOARD_LED_GREEN  /**< Scanning LED will be on when the device is scanning. */
#define CENTRAL_CONNECTED_LED        RUUVI_BOARD_LED_RED    /**< Connected LED will be on when the device is connected. */

/**@brief GPIO interrupt table for Ruuvi.
 * @ingroup leds_and_buttons
 */
static ruuvi_interface_gpio_interrupt_fp_t interrupt_table[RUUVI_BOARD_GPIO_NUMBER + 1] = {0};

/**@brief ADC sensor for Ruuvi.
 * @ingroup battery
 */
static ruuvi_driver_sensor_t adc_sensor = {0};


/*************** LEDs ****************/

#if NRF_MODULE_ENABLED(DEBUG)
void leds_init(void) {
    ruuvi_interface_gpio_id_t leds[RUUVI_BOARD_LEDS_NUMBER];
    ruuvi_driver_status_t err_code = RUUVI_DRIVER_SUCCESS;
    if(!ruuvi_interface_gpio_is_init())
        err_code = ruuvi_interface_gpio_init();
    if (RUUVI_DRIVER_SUCCESS == err_code) {
        uint16_t pins[] = RUUVI_BOARD_LEDS_LIST;

        for (uint8_t i = 0; RUUVI_BOARD_LEDS_NUMBER > i; ++i) {
            ruuvi_interface_gpio_id_t led;
            led.pin = pins[i];
            leds[i] = led;
            err_code |= ruuvi_interface_gpio_configure(leds[i], RUUVI_INTERFACE_GPIO_MODE_OUTPUT_HIGHDRIVE);
            err_code |= ruuvi_interface_gpio_write(leds[i], RUUVI_BOARD_LEDS_ACTIVE_STATE);
        }
    }

    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_SUCCESS);
}
#endif // NRF_MODULE_ENABLED(DEBUG)

void blesc_toggle_leds(bool scanning_led_state, bool connected_led_state) {
#if NRF_MODULE_ENABLED(DEBUG)
    ruuvi_interface_gpio_id_t pin;
    pin.pin = CENTRAL_SCANNING_LED;
    ruuvi_interface_gpio_write(pin, scanning_led_state ? RUUVI_INTERFACE_GPIO_LOW : RUUVI_INTERFACE_GPIO_HIGH);
    pin.pin = CENTRAL_CONNECTED_LED;
    ruuvi_interface_gpio_write(pin, connected_led_state ? RUUVI_INTERFACE_GPIO_LOW : RUUVI_INTERFACE_GPIO_HIGH);
    // Have to switch HIGH and LOW
#endif
}

/*************** Buttons ****************/

#if NRF_MODULE_ENABLED(DEBUG)

/**@brief Handler for RuuviTag button events.
 * @ingroup leds_and_buttons
 *
 * @param[in]  event    Pointer to the RuuviTag GPIO event to handle.
 */
static void ruuvi_button_event_handler(ruuvi_interface_gpio_evt_t event) {
    ret_code_t err_code;

    switch (event.pin.pin) {
    case RUUVI_BOARD_BUTTON_1:
        button_event_handler();
        break;
    }
}

void buttons_init(bool *p_erase_bonds) {
    ruuvi_driver_status_t err_code = RUUVI_DRIVER_SUCCESS;

    if (!(ruuvi_interface_gpio_is_init() && ruuvi_interface_gpio_interrupt_is_init())) {
        err_code = ruuvi_interface_gpio_init();
        RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_SUCCESS);
        err_code = ruuvi_interface_gpio_interrupt_init(interrupt_table, sizeof(interrupt_table));
        RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_SUCCESS);
    }
    ruuvi_interface_gpio_id_t pin = {.pin = RUUVI_BOARD_BUTTON_1};
    ruuvi_interface_gpio_slope_t slope = RUUVI_INTERFACE_GPIO_SLOPE_HITOLO;
    ruuvi_interface_gpio_mode_t mode = RUUVI_INTERFACE_GPIO_MODE_INPUT_PULLDOWN;

    if (RUUVI_INTERFACE_GPIO_LOW == RUUVI_BOARD_BUTTONS_ACTIVE_STATE) {
        mode = RUUVI_INTERFACE_GPIO_MODE_INPUT_PULLUP;
    }

    err_code = ruuvi_interface_gpio_interrupt_enable(pin, slope, mode, &ruuvi_button_event_handler);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_SUCCESS);
}
#endif // NRF_MODULE_ENABLED(DEBUG)

/*************** ADC ****************/

void adc_init(void) {
    ruuvi_driver_status_t err_code = RUUVI_DRIVER_SUCCESS;
    ruuvi_driver_bus_t bus = RUUVI_DRIVER_BUS_NONE;
    uint8_t handle = RUUVI_INTERFACE_ADC_AINVDD;
    ruuvi_driver_sensor_configuration_t config;

    config.samplerate    = APPLICATION_ADC_SAMPLERATE;
    config.resolution    = APPLICATION_ADC_RESOLUTION;
    config.scale         = APPLICATION_ADC_SCALE;
    config.dsp_function  = APPLICATION_ADC_DSPFUNC;
    config.dsp_parameter = APPLICATION_ADC_DSPPARAM;
    config.mode          = APPLICATION_ADC_MODE;

    err_code |= ruuvi_interface_adc_mcu_init(&adc_sensor, bus, handle);
    //RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_SUCCESS);

    err_code |= adc_sensor.configuration_set(&adc_sensor, &config);
    //RUUVI_DRIVER_ERROR_CHECK(err_code, ~RUUVI_DRIVER_ERROR_FATAL);
}

void battery_level_measure(void) {
    ret_code_t err_code;

    __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "Battery level measurement request.\r\n");

    float voltage_batt_lvl;
    uint8_t percentage_batt_lvl;

    ruuvi_driver_status_t ruuvi_err_code = RUUVI_DRIVER_SUCCESS;
    ruuvi_driver_sensor_data_t data = {0};
    data.data = &voltage_batt_lvl;
    data.fields.datas.voltage_v = 1;

    ruuvi_err_code |= adc_sensor.data_get(&data);
    if (RUUVI_DRIVER_SUCCESS != ruuvi_err_code) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Battery level measurement fail.\r\n");
        return;
    }

    battery_level_send(voltage_batt_lvl);
}   

/** @} */