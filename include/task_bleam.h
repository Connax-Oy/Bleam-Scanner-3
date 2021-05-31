/**
 * @addtogroup task_bleam
 * @{
 */
#ifndef BLESC_SERVICE_HANDLER_H__
#define BLESC_SERVICE_HANDLER_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

#include "bleam_service.h"
#include "bleam_send_helper.h"
#include "task_storage.h"
#include "task_fds.h"
#include "task_signature.h"

/**@brief Function for handling the data from the Bleam Service.
 * @ingroup bleam_connect
 *
 * @details This function will process the data received from the Bleam Service
 *
 * @param[in] p_bleam_client       Pointer to the struct of Bleam service.
 * @param[in] p_evt                Bleam Service event.
 *
 * @returns Nothing.
 */
void bleam_service_evt_handler(bleam_service_client_t *p_bleam_client, bleam_service_client_evt_t *p_evt);

/**@brief Function for aborting a device connection.
 *
 * @param[in] p_bleam_client       Pointer to Bleam Service client instance.
 *
 * @returns Nothing.
 */
void bleam_connection_abort(bleam_service_client_t *p_bleam_client);

#endif // BLESC_SERVICE_HANDLER_H__

/** @}*/