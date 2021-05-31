/**@defgroup global_app_config Global app configuration
 * @{
 *
 * @brief Macro definitions of Bleam Scanner aplication parameters.
 *
 * @details Most of definitions here fall under different modules.
 */

#ifndef GLOBAL_APP_CONFIG_H__
#define GLOBAL_APP_CONFIG_H__

#include <stdbool.h>

//#define HARDCODED_CONFIG 1

/** Enable debug for LEDs, logs and Bleam Scanner advertising */
#define DEBUG_ENABLED 0
#if !DEBUG_ENABLED || BOARD_IBKS_PLUS
  #ifdef NRF_LOG_DEFAULT_LEVEL
    #undef NRF_LOG_DEFAULT_LEVEL
  #endif
  #define NRF_LOG_DEFAULT_LEVEL 1 /**< NRF_LOG setting: only error messages */
  #define APP_CONFIG_LOG_LEVEL 1  /**< nRF Mesh log setting: only error messages */
#else // DEBUG_ENABLED
  #define APP_CONFIG_LOG_LEVEL 5 /**< nRF Mesh log setting: debug logging */
#endif

#define APP_CONFIG_DEVICE_NAME             "BLESc" /**< Name of device. Will be included in the advertising data. */
#define APP_CONFIG_PROTOCOL_NUMBER         3       /**< Bleam Scanner protocol number. */
#define APP_CONFIG_FW_VERSION_ID           13      /**< Firmware version ID. */

#define APP_CONFIG_BLEAM_SERVICE_UUID      0xB500  /**< @ingroup bleam_service
                                                     * UUID of Bleam device. */
#define APP_CONFIG_CONFIG_SERVICE_UUID     0xB700  /**< @ingroup blesc_config
                                                     *  UUID of Bleam Scanner configuration service. */

/** @}*/

/**@addtogroup bleam_time
 * @{
 */

#define APP_CONFIG_SCAN_CONNECT_INTERVAL    10000   /**< Maximum time Bleam Scanner can spend scanning before it tries to connect. */
#define APP_CONFIG_BLEAM_INACTIVITY_TIMEOUT 3000    /**< Maximum inactivity time after Bleam connection before Bleam Scanner disconnects. */
#define APP_CONFIG_MACLIST_TIMEOUT          30000   /**< Expiry timeout for MAC whitelist/blacklist entries */

// Period is a time segment bound to real time. All Bleam Scanners have to be awake at the start of each period.

#define BLESC_TIME_PERIOD_SECS            10        /**< Number of seconds in a period Bleam Scanner scanners will try to sync by */
#define BLESC_TIME_PERIODS_DAY            1         /**< Number of @ref BLESC_TIME_PERIOD_SECS in a day cycle, for systemwide sync */
#define BLESC_TIME_PERIODS_NIGHT          6         /**< Number of @ref BLESC_TIME_PERIOD_SECS in a night cycle, for systemwide sync */

#define APP_CONFIG_ECO_SCAN_SECS          1         /**< Time interval for Bleam Scanner to scan for BLEAMs between sleeps */

#define TIME_TO_SEC(_h, _m, _s)           (_h*60*60 + _m*60 + _s)  /**< Macro to convert 24-hour H:M:S time to seconds since midnight */
#define BLESC_DAYTIME_START               TIME_TO_SEC(6, 0, 0)     /**< System time that corresponds with start of the day */
#define BLESC_NIGHTTIME_START             TIME_TO_SEC(1, 0, 0)   /**< System time that corresponds with start of the night */

/** @} end of bleam_time */

/**@addtogroup bleam_storage
 * @{
 */

#define APP_CONFIG_MAX_BLEAMS           8       /**< Size of detected devices' RSSI data storage array */
#define APP_CONFIG_BLEAM_UUID_SIZE      10      /**< Length of the unique Bleam UUID part */
#define APP_CONFIG_RSSI_PER_MSG         5       /**< Number of RSSI scan results per message to Bleam */
#define APP_CONFIG_DATA_CHUNK_SIZE      16      /**< Length of a chunk of large data that can be sent in one message */

/** @} end of bleam_storage */

#define APP_CONFIG_MITJA_DEBUG_APPKEY     {0x44, 0xAD, 0x6B, 0x6D, 0x79, 0xFF, 0x8B, 0xAE, 0x5C, 0x21, 0x45, 0x16, 0x60, 0x4B, 0xD9, 0x06}
#define APP_CONFIG_TANYA_DEBUG_APPKEY     {0xCC, 0xE6, 0x39, 0xDD, 0xD7, 0xD0, 0x33, 0xB6, 0xF7, 0x78, 0x59, 0xAD, 0xCD, 0x8B, 0x2C, 0x59}
#define APP_CONFIG_PROD_APPKEY            {0x4D, 0xE9, 0xBB, 0x2E, 0x5B, 0xD8, 0x78, 0xEA, 0x07, 0xED, 0x30, 0xCB, 0x87, 0xB9, 0xDA, 0x49}
#define APP_CONFIG_HOME_DEBUG_APPKEY      {0xCC, 0xE2, 0x9D, 0x58, 0xE3, 0x5B, 0x67, 0x4C, 0xD1, 0x53, 0x27, 0xD2, 0x4C, 0x8C, 0x58, 0x57}

#define IOS_TESTING_BLESC_NODE_ID 0x0002
// 8DC813A6DB051F11AB41DBAF39EC401DFBA5EDC35C9D9409EC480D7E676F7284
#define IOS_TESTING_BLESC_PRIVATE {0x8D, 0xC8, 0x13, 0xA6, 0xDB, 0x05, 0x1F, 0x11, 0xAB, 0x41, 0xDB, 0xAF, 0x39, 0xEC, 0x40, 0x1D, 0xFB, 0xA5, 0xED, 0xC3, 0x5C, 0x9D, 0x94, 0x09, 0xEC, 0x48, 0x0D, 0x7E, 0x67, 0x6F, 0x72, 0x84}
// ED4121F358DABE1DB1AC650AF96D08BA300B9D323BAA9441E87230F382C400AC16ED33D2CB69C8B5A0BDD19B7C33FB80BED54F00B7F8CF9D0F8821AF2ACB9442
#define IOS_TESTING_BLESC_PUBLIC  {0xED, 0x41, 0x21, 0xF3, 0x58, 0xDA, 0xBE, 0x1D, 0xB1, 0xAC, 0x65, 0x0A, 0xF9, 0x6D, 0x08, 0xBA, 0x30, 0x0B, 0x9D, 0x32, 0x3B, 0xAA, 0x94, 0x41, 0xE8, 0x72, 0x30, 0xF3, 0x82, 0xC4, 0x00, 0xAC, 0x16, 0xED, 0x33, 0xD2, 0xCB, 0x69, 0xC8, 0xB5, 0xA0, 0xBD, 0xD1, 0x9B, 0x7C, 0x33, 0xFB, 0x80, 0xBE, 0xD5, 0x4F, 0x00, 0xB7, 0xF8, 0xCF, 0x9D, 0x0F, 0x88, 0x21, 0xAF, 0x2A, 0xCB, 0x94, 0x42}
// A3C8C9F3098BF5501A353046AEAE3B6B0A5904509DCBB38B3F55327C675DBB0A3DF3AE626C6519772B88526A964E027E95DDBAC47B80EB40C06D63B511E08753
#define IOS_TESTING_BLEAM_PUBLIC  {0xA3, 0xC8, 0xC9, 0xF3, 0x09, 0x8B, 0xF5, 0x50, 0x1A, 0x35, 0x30, 0x46, 0xAE, 0xAE, 0x3B, 0x6B, 0x0A, 0x59, 0x04, 0x50, 0x9D, 0xCB, 0xB3, 0x8B, 0x3F, 0x55, 0x32, 0x7C, 0x67, 0x5D, 0xBB, 0x0A, 0x3D, 0xF3, 0xAE, 0x62, 0x6C, 0x65, 0x19, 0x77, 0x2B, 0x88, 0x52, 0x6A, 0x96, 0x4E, 0x02, 0x7E, 0x95, 0xDD, 0xBA, 0xC4, 0x7B, 0x80, 0xEB, 0x40, 0xC0, 0x6D, 0x63, 0xB5, 0x11, 0xE0, 0x87, 0x53}


/**@addtogroup task_fds
 * @{
 */

#define APP_VERSION_FILE           (0x1111) /**< Version data FDS file ID */
#define APP_CONFIG_VERSION_REC_KEY (0x0001) /**< Version data FDS record key */
#define APP_CONFIG_PARAMS_FILE     (0x2222) /**< Parameters data FDS file ID */
#define APP_CONFIG_PARAMS_REC_KEY  (0x0002) /**< Parameters data FDS record key */
#define APP_CONFIG_FILE            (0x1234) /**< Configuration data FDS file ID */
#define APP_CONFIG_CONFIG_REC_KEY  (0x5789) /**< Configuration data FDS record key */

/** @} end of blecs_fds */

#endif /* GLOBAL_APP_CONFIG_H__ */