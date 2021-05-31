/**
 * @addtogroup task_fds
 * @{
 */
#ifndef BLESC_FDS_H__
#define BLESC_FDS_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

#include "fds.h"
#include "fds_internal_defs.h"
#ifdef SDK_12_3
  #include "fstorage.h"
#endif
#include "nrf_nvmc.h"

#include "task_storage.h"
#include "task_signature.h"

#ifndef FDS_PHY_PAGES_RESERVED
/** The number of physical pages at the end of the flash that are reserved by other modules. */
  #define FDS_PHY_PAGES_RESERVED     ((FDS_VIRTUAL_PAGES_RESERVED * FDS_VIRTUAL_PAGE_SIZE) / FDS_PHY_PAGE_SIZE)
#endif

/**@brief Function for loading config data from FDS, if there is any.
 *
 * @retval NRF_SUCCESS on success
 * @retval NRF_ERROR_NOT_FOUND if configuration data is not found on flash
 */
ret_code_t flash_config_load(void);

/**@brief Function for saving config data to FDS.
 *
 * @retval NRF_ERROR_INVALID_STATE if configuration data is already present on flash
 * @returns return value of @link_fds_record_write otherwise.
 */
ret_code_t flash_config_write(void);

/**@brief Function for resetting Bleam Scanner node after successful configuration.
 *
 * @retval NRF_SUCCESS on success
 * @retval NRF_ERROR_NOT_FOUND if configuration data is not found on flash
 */
ret_code_t flash_config_delete(void);

/**@brief Function for updating Bleam Scanner params data in flash
 *
 * @returns Nothing.
 */
void flash_params_update(void);

/**@brief Function for loading Bleam Scanner params data from FDS, if there is any.
 *
 * @retval NRF_SUCCESS on success
 * @retval NRF_ERROR_NOT_FOUND if Bleam Scanner params data is not found on flash
 */
ret_code_t flash_params_load(void);

/**@brief Function for emergency wiping FDS pages.
 *
 * @returns Nothing.
 */
void flash_pages_erase(void);

/**@brief Function for handling Flash Data Storage events.
 *
 * @param[in]     p_evt     FDS event.
 *
 * @returns Nothing.
 */
void fds_evt_handler(fds_evt_t const *p_evt);

/**@brief Function for initialising Flash Data Storage module.
 *
 * @returns Nothing.
 */
void flash_init(void (*cb)(void));

#endif // BLESC_FDS_H__

/** @}*/