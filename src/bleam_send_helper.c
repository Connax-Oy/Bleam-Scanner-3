/** @file bleam_send_helper.c
 *
 * @defgroup bleam_send_helper Bleam send helper
 * @{
 * @ingroup blesc_tasks
 * @ingroup bleam_service
 *
 * @brief Packaging data for sending to Bleam.
 */

#include "bleam_send_helper.h"
#include "nrf_crypto.h"
#include "log.h"

#include "task_signature.h"

/** RSSI data queue for Bleam */
static bleam_service_rssi_data_t bleam_rssi_queue[BLEAM_QUEUE_SIZE];
static uint16_t bleam_rssi_queue_front; /**< Index of the front element of the RSSI data queue */
static uint16_t bleam_rssi_queue_back;  /**< Index of the back element of the RSSI data queue */

/* Health data queue for Bleam */
static bleam_service_health_general_data_t health_general_message; /**< General health status data message struct. */
static bleam_service_health_error_info_t   health_error_info;      /**< Detailed error info message struct. */

static bleam_service_client_t *m_bleam_service_client;           /**< Pointer to Bleam service client instance */
static uint16_t               m_bleam_send_char;                 /**< Characteristic to write to */
static uint8_t                m_data_index;                      /**< Index inside data array */
static uint8_t                m_signature[BLESC_SIGNATURE_SIZE]; /**< Pointer to array with signature to send */
static uint8_t                m_signature_size;                  /**< Size of data to send as signature */

/* Forward declaration */
static void bleam_send_health(void);
static void bleam_send_rssi(void);

/**@brief Function for writing data to Bleam.
 *
 *@details This function sends the contents of p_data_array[]
 *         of size p_data_len over to bleam_service to be sent to Bleam.
 *         Function should only be called after connection to Bleam is established
 *         and the @ref m_bleam_send_char characteristic is discovered.
 *
 * @returns Nothing.
 */
static void bleam_send_write_data(uint8_t * p_data_array, uint16_t p_data_len) {
    ret_code_t err_code = NRF_SUCCESS;
    if (0 == p_data_len) {
        return;
    }
    err_code = bleam_service_data_send(m_bleam_service_client, p_data_array, p_data_len, m_bleam_send_char);
    if (err_code != NRF_ERROR_INVALID_STATE) {
        APP_ERROR_CHECK(err_code);
    } else {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "BLEAM_send_write_data NRF_ERROR_INVALID_STATE\r\n");
    }
}

/**@brief Function for fragmenting signature to send to Bleam.
 *
 *@details Function for sending signature hash over to Bleam.
 *         It takes data from signature array @ref m_signature[]
 *         and packs it into an array.
 *         When the array is full, it calls @ref bleam_send_write_data() to write this data.
 *
 * @returns Nothing.
 */
static void bleam_send_signature_chunks(void) {
    if(m_signature_size <= m_data_index * APP_CONFIG_DATA_CHUNK_SIZE) {
        m_bleam_send_char = BLEAM_S_HEALTH;

        bleam_service_client_evt_t evt;
        evt.evt_type = BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_SIGNATURE;
        m_bleam_service_client->evt_handler(m_bleam_service_client, &evt);
        return;
    }

    uint16_t data_size = BLEAM_S_MSG_SIZE_SIGN;
    uint8_t data_array[BLEAM_S_MSG_SIZE_SIGN] = {0};

    data_array[0] = m_data_index + 1;
    memcpy(data_array + 1, m_signature + (m_data_index * APP_CONFIG_DATA_CHUNK_SIZE), APP_CONFIG_DATA_CHUNK_SIZE);
    ++m_data_index;

    bleam_send_write_data(data_array, data_size);
}

/**@brief Function for assembling health data to send to Bleam.
 *
 * @returns Nothing.
 */
static void bleam_send_health(void) {
    if(0 == health_general_message.msg_type && 0 == health_error_info.msg_type) {
        m_bleam_send_char = BLEAM_S_RSSI;

        bleam_service_client_evt_t evt;
        evt.evt_type = BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_HEALTH;
        m_bleam_service_client->evt_handler(m_bleam_service_client, &evt);
        return;
    }

    uint8_t data_array[BLEAM_MAX_DATA_LEN] = {0};
    uint16_t msg_len = 0;

    if(0 != health_general_message.msg_type) {
        msg_len = sizeof(bleam_service_health_general_data_t);
        memcpy(data_array, (uint8_t *)(&health_general_message), msg_len);
        memset(&health_general_message, 0, msg_len);
    } else if (0 != health_error_info.msg_type) {
        msg_len = sizeof(bleam_service_health_error_info_t);
        memcpy(data_array, (uint8_t *)(&health_error_info), msg_len);
        memset(&health_error_info, 0, msg_len);
    }

    m_bleam_send_char = BLEAM_S_HEALTH;
    bleam_send_write_data(data_array, msg_len);
}

/**@brief Function for assembling RSSI data to send to Bleam.
 *
 * @returns Nothing.
 */
static void bleam_send_rssi(void) {
    uint8_t rssi_in_msg = BLEAM_MAX_RSSI_PER_MSG;
    if(bleam_rssi_queue_back == bleam_rssi_queue_front) {
        bleam_rssi_queue_back = bleam_rssi_queue_front = 0;
        m_bleam_send_char = BLEAM_CHAR_FINAL;

        bleam_service_client_evt_t evt;
        evt.evt_type = BLEAM_SERVICE_CLIENT_EVT_DONE_SENDING_RSSI;
        m_bleam_service_client->evt_handler(m_bleam_service_client, &evt);
        return;
    } else if(bleam_rssi_queue_back > bleam_rssi_queue_front &&
            bleam_rssi_queue_back - bleam_rssi_queue_front < BLEAM_MAX_RSSI_PER_MSG) {
        rssi_in_msg = bleam_rssi_queue_back - bleam_rssi_queue_front;
    } else if(bleam_rssi_queue_back < bleam_rssi_queue_front &&
            BLEAM_QUEUE_SIZE - bleam_rssi_queue_front < BLEAM_MAX_RSSI_PER_MSG) {
        rssi_in_msg = BLEAM_QUEUE_SIZE - bleam_rssi_queue_front;
    }

    uint16_t data_size = rssi_in_msg * sizeof(bleam_service_rssi_data_t);
    uint8_t data_array[BLEAM_MAX_DATA_LEN] = {0};
    memcpy(data_array, (uint8_t *)(bleam_rssi_queue + bleam_rssi_queue_front), data_size);

    m_bleam_send_char = BLEAM_S_RSSI;
    bleam_send_write_data(data_array, data_size);
    bleam_rssi_queue_front += rssi_in_msg;
    bleam_rssi_queue_front %= BLEAM_QUEUE_SIZE;
}

/********************************** INTERFACE *********************************/

void bleam_send_init(bleam_service_client_t *p_bleam_service_client) {
    m_bleam_service_client = p_bleam_service_client;
}

void bleam_send_signature(uint8_t *p_signature, size_t p_size) {
    m_data_index           = 0;
    m_signature_size       = p_size;
    m_bleam_send_char      = BLEAM_S_SIGN;
    memcpy(m_signature, p_signature, m_signature_size);
    bleam_send_signature_chunks();
}

void bleam_send_uninit(void) {
    m_bleam_service_client = NULL;
    m_signature_size       = 0;
    m_bleam_send_char      = BLEAM_CHAR_EMPTY;
    bleam_rssi_queue_front = 0;
    bleam_rssi_queue_back  = 0;
}

void bleam_send_continue(void) {
    // If sending signature, finish with signature.
    // Otherwise send all the health first, then RSSI
    switch (m_bleam_send_char) {
    case BLEAM_S_SIGN:
        bleam_send_signature_chunks(); 
        break;
    case BLEAM_CHAR_EMPTY:
        m_bleam_send_char = BLEAM_S_HEALTH;
    case BLEAM_S_HEALTH:
        bleam_send_health();
        break;
    case BLEAM_S_RSSI:
        bleam_send_rssi();
        break;
    }
}

void bleam_rssi_queue_add(int8_t rssi, uint8_t aoa) {
    bleam_rssi_queue[bleam_rssi_queue_back].rssi = rssi;
    bleam_rssi_queue[bleam_rssi_queue_back].aoa = aoa;
    bleam_rssi_queue_back = (bleam_rssi_queue_back + 1) % BLEAM_QUEUE_SIZE;
    if(bleam_rssi_queue_back == bleam_rssi_queue_front)
        bleam_rssi_queue_front = (bleam_rssi_queue_front + 1) % BLEAM_QUEUE_SIZE;
}

void bleam_health_queue_add(uint8_t battery_lvl, uint32_t uptime, uint32_t system_time, uint32_t sleep_time_sum) {
    health_general_message.msg_type    = 0x01;
    health_general_message.battery_lvl = battery_lvl;
    health_general_message.fw_id       = APP_CONFIG_FW_VERSION_ID;
    health_general_message.uptime      = uptime;
    health_general_message.system_time = system_time;
    health_general_message.sleep_time  = sleep_time_sum;

    blesc_retained_error_t blesc_error = blesc_error_get();

    health_general_message.err_id      = blesc_error.random_id;
    health_general_message.err_type    = blesc_error.error_type;

    // Only these error types require a detailed error message
    if(BLESC_ERR_T_SDK_ASSERT == blesc_error.error_type || BLESC_ERR_T_SDK_ERROR == blesc_error.error_type) {
        health_error_info.msg_type = 0x02;
        health_error_info.err_code = blesc_error.error_info.err_code;
        health_error_info.line_num = blesc_error.error_info.line_num;
        memcpy(health_error_info.file_name, blesc_error.error_info.file_name, BLESC_ERR_FILE_NAME_SIZE);
    } else {
        health_error_info.msg_type = 0x00;
    }

    if((BLEAM_S_HEALTH == m_bleam_send_char || BLEAM_CHAR_EMPTY == m_bleam_send_char) && NULL != m_bleam_service_client) {
        bleam_send_continue();
    }
}

/** @}*/