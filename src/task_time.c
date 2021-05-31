/** @file task_time.c
 *
 * @defgroup task_time Task Time
 * @{
 * @ingroup bleam_time
 * @ingroup blesc_tasks
 *
 * @brief System time maintanence.
 */
#include "task_time.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#include "task_board.h"
#include "task_scan_connect.h"

static uint32_t m_system_time;          /**< Bleam Scanner system time in seconds passed since midnight */
static uint32_t m_blesc_uptime;         /**< Node uptime in minutes since last boot */
static uint32_t m_blesc_wakeup_uptime;  /**< Uptime at which Bleam Scanner has to wake up from IDLE request */
static uint32_t m_blesc_time_period;    /**< Scan period: maximum between scans */
static uint32_t m_blesc_sleep_time_sum; /**< Amount of time Bleam Scanner node had spent idling in seconds. */
static uint32_t m_blesc_sleep_timestamp; /**< System time value at which node went to idle. */
static bool m_system_time_needs_update; /**< Flag that denoted that system time needs to be updated */

APP_TIMER_DEF(m_system_time_timer_id);  /**< Timer for updating system time. */


/************ Data manipulation and helper functions ************/

void system_time_update(uint32_t bleam_time) {
    m_system_time = bleam_time / 1000;
    m_system_time_needs_update = false;
}

uint32_t get_system_time(void) {
    return m_system_time;
}

uint32_t get_blesc_uptime(void) {
    return m_blesc_uptime;
}

bool system_time_needs_update_get(void) {
    return m_system_time_needs_update;
}

void system_time_needs_update_set(void) {
    m_system_time_needs_update = true;
}

uint32_t get_sleep_time_sum(void) {
    return m_blesc_sleep_time_sum;
}

/************ System time ************/

void blesc_set_idle_time_minutes(uint32_t minutes) {
    m_blesc_wakeup_uptime = m_blesc_uptime + minutes;
}

/**@brief Function to increment system time and perform actions that depend on current time.
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 *
 * @returns Nothing.
 */
static void system_time_increment(void * p_context) {
    // This timer interrupt feeds watchdog even if Bleam Scanner is idle
    ++m_system_time;
    if (m_system_time >= 24 * 60 * 60) {
        m_system_time = 0;
        m_system_time_needs_update = true;
    }
    if(0 == m_system_time % 60) {
        ++m_blesc_uptime;
    }
    if(blesc_node_state_get() == BLESC_STATE_IDLE) {
        ++m_blesc_sleep_time_sum;
    }

    if (BLESC_DAYTIME_START == m_system_time) {
        m_blesc_time_period = BLESC_TIME_PERIODS_DAY * BLESC_TIME_PERIOD_SECS;
    } else if (BLESC_NIGHTTIME_START == m_system_time) {
        m_blesc_time_period = BLESC_TIME_PERIODS_NIGHT * BLESC_TIME_PERIOD_SECS;
    }

    // try start scan every period
    if (0 == m_system_time % m_blesc_time_period &&
            BLESC_STATE_IDLE == blesc_node_state_get() &&
            m_blesc_wakeup_uptime < m_blesc_uptime) {
        eco_timer_handler(NULL);
    }
}

void system_time_init(void) {
    m_system_time              = BLESC_DAYTIME_START;
    m_blesc_uptime             = 1;
    m_blesc_wakeup_uptime      = 0;
    m_blesc_time_period        = BLESC_TIME_PERIODS_DAY * BLESC_TIME_PERIOD_SECS;
    m_system_time_needs_update = true;

    // System time timer.
    ret_code_t err_code = app_timer_create(&m_system_time_timer_id, APP_TIMER_MODE_REPEATED, system_time_increment);
    APP_ERROR_CHECK(err_code);

    app_timer_start(m_system_time_timer_id, __TIMER_TICKS(1000), NULL);
}

/** @}*/