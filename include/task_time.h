/**
 * @addtogroup task_time
 * @{
 */
#ifndef TASK_TIME_H__
#define TASK_TIME_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

/**@brief Function to update system time value.
 *
 * @param[in]  bleam_time   New system time value.
 *
 * @returns Nothing.
 */
void system_time_update(const uint32_t bleam_time);

/**@brief Function to provide external modules with system time value.
 *
 * @returns Bleam Scanner system time value.
 */
uint32_t get_system_time(void);

/**@brief Function to provide external modules with uptime value.
 *
 * @returns Bleam Scanner uptime value.
 */
uint32_t get_blesc_uptime(void);

/**@brief Function to to set wakeup time for Bleam Scanner IDLE state on request.
 *
 * @details This function sets a future value for @ref m_blesc_wakeup_uptime
 *          for @ref m_blesc_uptime to reach. It is called on confirmation
 *          of Bleam request to IDLE for set amount of minutes.
 *
 * @param[in]  minutes      Amount of minutes Bleam Scanner will IDLE for.
 *
 * @returns Nothing.
 */
void blesc_set_idle_time_minutes(uint32_t minutes);

/**@brief Function to provide external modules with information on
 *         whether system time needs to be updated.
 *
 * @retval true if @ref m_system_time_needs_update is also true.
 * @retval false otherwise
 */
bool system_time_needs_update_get(void);

/**@brief Function to mark Bleam Scanner system time as requiring to be updated.
 *
 * @returns Nothing.
 */
void system_time_needs_update_set(void);

/**@brief Function to provide external modules with sleep time sum value.
 *
 * @returns Bleam Scanner sleep time sum value.
 */
uint32_t get_sleep_time_sum(void);

/**@brief Function to initialise system time maintenance.
 *
 * @returns Nothing.
 */
void system_time_init(void);

#endif // TASK_TIME_H__

/** @}*/