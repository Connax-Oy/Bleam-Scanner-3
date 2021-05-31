/**
 * @addtogroup blesc_error
 * @{
 */

#ifndef BLESC_ERROR_H__
#define BLESC_ERROR_H__

/**@brief Health error type values. */
typedef enum {
    BLESC_ERR_T_HARD_RESET = 0x00,  /**< Bleam Scanner never experienced an error since latest hard reset. */
    BLESC_ERR_T_SD_ASSERT  = 0x01,  /**< SoftDevice assertion. */
    BLESC_ERR_T_APP_MEMACC = 0x02,  /**< Invalid memory access. */
    BLESC_ERR_T_SOFT_RESET = 0x0F,  /**< Soft reset without error. */
    BLESC_ERR_T_SDK_ASSERT = 0x11,  /**< Call to ASSERT in SDK failed: invalid crucial parameter value. */
    BLESC_ERR_T_SDK_ERROR  = 0x12,  /**< Call to APP_ERROR_CHECK failed: function returned error status. */
    BLESC_ERR_T_UNKNOWN    = 0xFF,  /**< Unknown error. */
} blesc_error_t;

#define BLESC_ERR_FILE_NAME_SIZE 13 /**< Max size of file name string to be saved in error info. */

/** @brief Health error info struct
 */
typedef struct __attribute((packed)) {
    uint32_t err_code;                            /**< The error code representing the error that occurred (from nRF SDK). */
    uint16_t line_num;                            /**< The line number where the error occurred. */
    uint8_t  file_name[BLESC_ERR_FILE_NAME_SIZE]; /**< The file in which the error occurred (first 13 symbols) */
} blesc_health_error_info_t;

/**@brief Blesc error info struct for RAM retention.
 * 
 * @returns Nothing.
 */
typedef struct __attribute((packed)) {
    uint16_t                  random_id;  /**< Random ID of the error record. */
    blesc_error_t             error_type; /**< Health error type value. */
    blesc_health_error_info_t error_info; /**< Detailed error info. */
} blesc_retained_error_t;

/**@brief Check retained error data after system boot.
 * 
 * @returns Nothing.
 */
void blesc_error_on_boot(void);

/**@brief Get retained error value.
 *
 * @returns contents of the retained error structure.
 */
blesc_retained_error_t blesc_error_get(void);

#endif // BLESC_ERROR_H__

/** @}*/