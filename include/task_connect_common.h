/**
 * @addtogroup task_connect_common
 * @{
 */
#ifndef TASK_CONNECT_COMMON_H__
#define TASK_CONNECT_COMMON_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

/**@brief Function to start inactivity timer.
 *
 * @returns Nothing.
 */
void bleam_inactivity_timer_start(void);

/**@brief Function to stop inactivity timer.
 *
 * @returns Nothing.
 */
void bleam_inactivity_timer_stop(void);


/**@brief Function to start watchdog timer.
 *
 * @returns Nothing.
 */
void idle_watchdog_timer_start(void);

/**@brief Function to stop watchdog timer.
 *
 * @returns Nothing.
 */
void idle_watchdog_timer_stop(void);


/**@brief Function to clear registered chunks array.
 *
 * @returns Nothing.
 */
void recvd_chunks_clear(void);

/**@brief Function to increment registered chunks array element.
 *
 * @param[in] index       Number of chunk received, counting from 0.
 *
 * @returns Nothing.
 */
void recvd_chunks_add(size_t index);

/**@brief Function to validate registered chunks.
 *
 * @details This function checks if every expected chunk has been received exactly once.
 *
 * @param[in] p_size      Number of chunks expected to be received.
 *
 * @retval true if every expected chunk has been received once
 * @retval false otherwise.
 */
bool recvd_chunks_validate(size_t p_size);

/**@brief Function to initialise common connect elements.
 *
 * @returns Nothing.
 */
void connect_common_init(void);

#endif // TASK_CONNECT_COMMON_H__

/** @}*/