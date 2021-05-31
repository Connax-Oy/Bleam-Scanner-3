/**
 * @addtogroup bleam_send_helper
 * @{
 */

#ifndef BLEAM_SEND_HELPER_H__
#define BLEAM_SEND_HELPER_H__

#include "bleam_service.h"

#define BLEAM_QUEUE_SIZE 20 /**< Size of the queue array */

#define BLEAM_MAX_RSSI_PER_MSG (BLEAM_MAX_DATA_LEN / sizeof(bleam_service_rssi_data_t)) /**< Maximum amount of RSSI entries in a single message to Bleam */

/**@brief Function for initialising parameters for sending data to Bleam.
 *
 * @param[in] p_bleam_service_client     Pointer to the struct of Bleam service.
 *
 * @returns Nothing.
 */
void bleam_send_init(bleam_service_client_t *p_bleam_service_client);

/**@brief Function for initialising parameters for and starting sending signature chunks to Bleam.
 *
 * @param[in] p_signature     Pointer to the array with signature or salt to send over to Bleam.
 * @param[in] p_size          Size of the array with signature or salt to send over to Bleam.
 *
 * @returns Nothing.
 */
void bleam_send_signature(uint8_t *p_signature, size_t p_size);

/**@brief Function for deinitialising parameters for sending data to Bleam.
 *
 * @returns Nothing.
 */
void bleam_send_uninit(void);

/**@brief Function for continuing with assembling and sending data
 * after previous send is confirmed to be over.
 *
 * @returns Nothing.
 */
void bleam_send_continue(void);

/**@brief Function for initialising parameters for and sending salt to Bleam.
 *
 * @param[in] rssi          Received Signal Strength of Bleam.
 * @param[in] aoa           Angle of arrival of Bleam signal.
 *
 * @returns Nothing.
 */
void bleam_rssi_queue_add(int8_t rssi, uint8_t aoa);

/**@brief Function for initialising parameters for and sending salt to Bleam.
 *
 * @param[in] battery_lvl       Battery level in centivolts.
 * @param[in] uptime            Bleam Scanner uptime in minutes.
 * @param[in] system_time       Bleam Scanner current system time value in seconds.
 * @param[in] sleep_time_sum    Bleam Scanner overall sleep time since startup in seconds.
 *
 * @returns Nothing.
 */
void bleam_health_queue_add(uint8_t battery_lvl, uint32_t uptime, uint32_t system_time, uint32_t sleep_time_sum);

#endif // BLEAM_SEND_HELPER_H__

/** @}*/