/** @file task_connect_common.c
 *
 * @defgroup task_connect_common Task Connect Common
 * @{
 * @ingroup bleam_connect
 * @ingroup blesc_config
 * @ingroup blesc_tasks
 *
 * @brief Common connection elements between Bleam service and Configuration service
 */
#include "task_scan_connect.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#include "task_board.h"

uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */
static uint8_t m_chunks_regist[4];                /**< Array for received chunks registration */

APP_TIMER_DEF(m_bleam_inactivity_timer_id); /**< Bleam timeout. */


/********************* Bleam inactivity timer *********************/

/**@brief Function for handling the Bleam timeout.
 *
 * @details This function will be called each time the Bleam inactivity timer expires.
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 *
 * @returns Nothing.
 */
static void bleam_inactivity_timeout_handler(void *p_context) {
    if(BLESC_STATE_CONNECT != blesc_node_state_get())
        return;
    if(m_conn_handle != BLE_CONN_HANDLE_INVALID) {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Bleam timeout\r\n");
        ret_code_t err_code = sd_ble_gap_disconnect(m_conn_handle,
            BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if(NRF_ERROR_INVALID_STATE != err_code)
            APP_ERROR_CHECK(err_code);
        return;
    }
}

void bleam_inactivity_timer_start(void) {
    ret_code_t err_code = app_timer_start(m_bleam_inactivity_timer_id, BLEAM_SERVICE_BLEAM_INACTIVITY_TIMEOUT, NULL);
    APP_ERROR_CHECK(err_code);
}

void bleam_inactivity_timer_stop(void) {
    app_timer_stop(m_bleam_inactivity_timer_id);
}

/********************* Chunk validation ***********************/

void recvd_chunks_clear(void) {
    memset(m_chunks_regist, 0, sizeof(m_chunks_regist));
}

void recvd_chunks_add(size_t index) {
    if(sizeof(m_chunks_regist) > index)
        ++m_chunks_regist[index];
}

bool recvd_chunks_validate(size_t p_size) {
    for(size_t index = 0; p_size > index; ++index) {
        if(1 != m_chunks_regist[index]) {
            return false;
        }
    }
    return true;
}

/***************** Init *****************/

void connect_common_init(void) {
    ret_code_t err_code = NRF_SUCCESS;

    // Bleam inactivity timer.
    err_code = app_timer_create(&m_bleam_inactivity_timer_id, APP_TIMER_MODE_SINGLE_SHOT, bleam_inactivity_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

/** @}*/