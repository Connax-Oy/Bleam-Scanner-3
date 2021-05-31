/**
 * @addtogroup task_config
 * @{
 */
#ifndef BLESC_CONFIGURATION_H__
#define BLESC_CONFIGURATION_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

#include "config_service.h"
#include "task_fds.h"

/**@brief Configuration Service FAIL detail status type. */
typedef enum {
    CONFIG_S_NO_FAIL,      /**< No Configuration Service fail detected. */
    CONFIG_S_FAIL_BLEAM,   /**< Configuration fail reported by Bleam Tools device. */
    CONFIG_S_FAIL_DATA,    /**< Invalid data received. */
    CONFIG_S_FAIL_FDS,     /**< Error while writing configuration data to flash. */
    CONFIG_S_FAIL_TIMEOUT, /**< Bleam Tools did not respond. */
} config_s_fail_status_t;

/**@brief Function for initializing the Advertising functionality.
 *
 * @returns Nothing.
 */
void advertising_init(void);

/**@brief Function for handling the Configuration Service events.
 *
 * @details This function will be called for all Configuration Service events which are passed to
 *          the application.
 *
 * @param[in]   p_config_s_server Configuration Service structure.
 * @param[in]   p_evt             Event received from the Configuration Service.
 *
 * @returns Nothing.
 */
void config_s_event_handler(config_s_server_t *p_config_s_server, config_s_server_evt_t const *p_evt);

/**@brief Function for initializing services that will be used by unconfigured Bleam Scanner.
 *
 * @param[in]   p_config_s_server Configuration Service structure.
 *
 * @returns Nothing.
 */
void config_mode_services_init(config_s_server_t * p_config_s_server);

/**@brief Function for providing external modules with Bleam Scanner node ID value.
 *
 * @returns Current Bleam Scanner node ID.
 */
uint16_t blesc_node_id_get(void);

/**@brief Function for providing external modules with Bleam Scanner node ID value.
 *
 * @returns Current Bleam Scanner params.
 */
blesc_params_t * blesc_params_get(void);

/**@brief Function for providing external modules with Bleam Scanner keys values.
 *
 * @returns Pointer to structure with Bleam Scanner keys.
 */
blesc_keys_t * blesc_keys_get(void);

/**@brief Wrapper function for updating STATUS char of Configuration Service.
 *
 * @param[in]   p_status   New status value.
 *
 * @returns Nothing.
 */
void config_status_update(config_s_status_t p_status);

#endif // BLESC_CONFIGURATION_H__

/** @}*/