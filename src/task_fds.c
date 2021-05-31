/** @file task_fds.c
 *
 * @defgroup task_fds Task FDS
 * @{
 * @ingroup bleam_storage
 * @ingroup blesc_tasks
 *
 * @brief Handles flash manipulations.
 */
#include "task_fds.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#ifdef BLESC_DFU
#include "nrf_bootloader_info.h"
#endif
#include "nrf_delay.h"
#include "task_board.h"
#include "task_config.h"
#include "task_scan_connect.h"

/** Bootloader address definition in case it is not defined elsewhere */
#if !defined(BOOTLOADER_ADDRESS)
  #if defined(BLESC_DFU)
    #define BOOTLOADER_ADDRESS NRF_UICR_BOOTLOADER_START_ADDRESS
  #else // !defined(BLESC_DFU)
    #define BOOTLOADER_ADDRESS (0xFFFFFFFF)
  #endif
#endif

static bool volatile m_fds_initialized;           /**< Flag to check fds initialization. */
__ALIGN(4) static version_t m_version_data = {0}; /**< Bleam Scanner version data */
__ALIGN(4) configuration_t  m_blesc_config = {0}; /**< Bleam Scanner configuration data */
__ALIGN(4) blesc_params_t   m_blesc_params = {
    .rssi_lower_limit = RSSI_LOWER_LIMIT_DEFAULT,
};                                                /**< Bleam Scanner params */

/** Function pointer for continuing BLESC initialization in main */
void (*init_finalize)(void);

/**@brief Function for checking version data in FDS and updating it, if needed
 *
 * @param[in] p_version_data       Pointer to version data.
 *
 * @retval NRF_SUCCESS on success
 * @retval NRF_ERROR_INVALID_STATE if protocol number changed after update
 * @retval NRF_ERROR_INVALID_DATA if firmware version changed after update
 */
static ret_code_t flash_version_update(version_t * p_version_data) {
    fds_record_desc_t desc_version = {0};
    fds_find_token_t  tok_version  = {0};

    ret_code_t err_code = fds_record_find(APP_CONFIG_FILE, APP_CONFIG_VERSION_REC_KEY, &desc_version, &tok_version);
    bool version_data_present = (FDS_SUCCESS == err_code);

    if(version_data_present) {
        fds_flash_record_t version_record = {0};
        err_code = fds_record_open(&desc_version, &version_record);
        APP_ERROR_CHECK(err_code);
        memcpy(p_version_data, version_record.p_data, sizeof(version_t));
        err_code = fds_record_close(&desc_version);
        APP_ERROR_CHECK(err_code);

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Version data from FDS: p %u -> %u, fw %u -> %u\r\n",
                                            p_version_data->protocol_id, APP_CONFIG_PROTOCOL_NUMBER,
                                            p_version_data->fw_id, APP_CONFIG_FW_VERSION_ID);
        if (APP_CONFIG_PROTOCOL_NUMBER != p_version_data->protocol_id) {
            // If protocol changed, configuration has to go
            fds_record_desc_t desc_config = {0};
            fds_find_token_t  tok_config  = {0};
            err_code = fds_record_find(APP_CONFIG_FILE, APP_CONFIG_CONFIG_REC_KEY, &desc_config, &tok_config);
            if(FDS_SUCCESS == err_code) {
                return NRF_ERROR_INVALID_STATE;
            }
            // If config record doesn't exist, just overwrite the version data
        } else if (APP_CONFIG_FW_VERSION_ID == p_version_data->fw_id) {
            // If version data is unchanged, nothing to do here
            return NRF_SUCCESS;
        }
        // Else proceed to version data overwrite
    }

    // Prepare version data to write
    p_version_data->fw_id       = APP_CONFIG_FW_VERSION_ID;
    p_version_data->protocol_id = APP_CONFIG_PROTOCOL_NUMBER;
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Version data to write: p %u fw %u\r\n", p_version_data->protocol_id, p_version_data->fw_id);

#ifdef SDK_12_3
    fds_record_chunk_t record_chunk;
    record_chunk.p_data = p_version_data;
    record_chunk.length_words = 1;
#endif

    fds_record_t const version_record = {
        .file_id = APP_CONFIG_FILE,
        .key = APP_CONFIG_VERSION_REC_KEY,
#if defined(SDK_15_3)
        .data.p_data       = p_version_data,
        .data.length_words = (sizeof(version_t) + 3) / sizeof(uint32_t),
#endif
#if defined(SDK_12_3)
        .data.p_chunks   = &record_chunk,
        .data.num_chunks = 1,
#endif
    };

    if (version_data_present) {
        err_code = fds_record_update(&desc_version, &version_record);
    } else {
        err_code = fds_record_write(&desc_version, &version_record);
    }
    APP_ERROR_CHECK(err_code);
    return NRF_ERROR_INVALID_DATA;
}

ret_code_t flash_config_load(void) {
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    ret_code_t err_code = fds_record_find(APP_CONFIG_FILE, APP_CONFIG_CONFIG_REC_KEY, &desc, &tok);

    if (FDS_SUCCESS != err_code) {
        return NRF_ERROR_NOT_FOUND;
    }

    fds_flash_record_t config = {0};
    err_code = fds_record_open(&desc, &config);
    APP_ERROR_CHECK(err_code);
    memcpy(&m_blesc_config, config.p_data, sizeof(configuration_t));
    err_code = fds_record_close(&desc);
    APP_ERROR_CHECK(err_code);

    return NRF_SUCCESS;
}

ret_code_t flash_config_write(void) {
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    ret_code_t err_code = fds_record_find(APP_CONFIG_FILE, APP_CONFIG_CONFIG_REC_KEY, &desc, &tok);

    if (FDS_SUCCESS == err_code) {
        // Unconfigured node shouldn't have this record
        return NRF_ERROR_INVALID_STATE;
    }

#ifdef SDK_12_3
    fds_record_chunk_t record_chunk;
    record_chunk.p_data = &m_blesc_config;
    record_chunk.length_words = 25;
#endif

    fds_record_t const config_record = {
        .file_id = APP_CONFIG_FILE,
        .key = APP_CONFIG_CONFIG_REC_KEY,
#if defined(SDK_15_3)
        .data.p_data       = &m_blesc_config,
        .data.length_words = (sizeof(configuration_t) + 3) / sizeof(uint32_t),
#endif
#if defined(SDK_12_3)
        .data.p_chunks   = &record_chunk,
        .data.num_chunks = 1,
#endif
    };
    
    err_code = fds_record_write(&desc, &config_record);
    APP_ERROR_CHECK(err_code);
    return err_code;
}

ret_code_t flash_config_delete(void) {
    // Erase config data from flash
    fds_find_token_t tok = {0};
    fds_record_desc_t desc = {0};

    if (FDS_SUCCESS == fds_record_find(APP_CONFIG_FILE, APP_CONFIG_CONFIG_REC_KEY, &desc, &tok)) {
        ret_code_t err_code = fds_record_delete(&desc);
        APP_ERROR_CHECK(err_code);
        return NRF_SUCCESS;
        // the rest in is @ref fds_evt_handler under FDS_EVT_DEL_RECORD event
    }
    return NRF_ERROR_NOT_FOUND;
}

void flash_params_update(void) {
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    ret_code_t err_code = fds_record_find(APP_CONFIG_PARAMS_FILE, APP_CONFIG_PARAMS_REC_KEY, &desc, &tok);
    bool param_record_exists = (FDS_SUCCESS == err_code);

#ifdef SDK_12_3
    fds_record_chunk_t record_chunk;
    record_chunk.p_data = &m_blesc_params;
    record_chunk.length_words = 1;
#endif

    fds_record_t const params_record = {
        .file_id = APP_CONFIG_PARAMS_FILE,
        .key = APP_CONFIG_PARAMS_REC_KEY,
#if defined(SDK_15_3)
        .data.p_data       = &m_blesc_params,
        .data.length_words = (sizeof(blesc_params_t) + 3) / sizeof(uint32_t),
#endif
#if defined(SDK_12_3)
        .data.p_chunks   = &record_chunk,
        .data.num_chunks = 1,
#endif
    };
    
    if(param_record_exists) {
        err_code = fds_record_update(&desc, &params_record);
        APP_ERROR_CHECK(err_code);
    } else {
        err_code = fds_record_write(&desc, &params_record);
        APP_ERROR_CHECK(err_code);
    }
}

ret_code_t flash_params_load(void) {
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    ret_code_t err_code = fds_record_find(APP_CONFIG_PARAMS_FILE, APP_CONFIG_PARAMS_REC_KEY, &desc, &tok);

    if (FDS_SUCCESS != err_code) {
        memset(&m_blesc_params, 0, sizeof(blesc_params_t));
        m_blesc_params.rssi_lower_limit = RSSI_LOWER_LIMIT_DEFAULT;
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Params record not found, set to default.\r\n");
        return NRF_ERROR_NOT_FOUND;
    }

    fds_flash_record_t params_record = {0};
    err_code = fds_record_open(&desc, &params_record);
    APP_ERROR_CHECK(err_code);
    memcpy(&m_blesc_params, params_record.p_data, sizeof(blesc_params_t));
    err_code = fds_record_close(&desc);
    APP_ERROR_CHECK(err_code);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "RSSI lower limit is %d.\r\n", m_blesc_params.rssi_lower_limit);
    return NRF_SUCCESS;
}

void fds_evt_handler(fds_evt_t const *p_evt) {
    ret_code_t err_code;

    switch (p_evt->id) {
    case FDS_EVT_INIT:
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "FDS event: INIT\r\n");
        
        ASSERT(p_evt->result == FDS_SUCCESS);
        m_fds_initialized = true;

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Looking for version data record...\r\n");
        err_code = flash_version_update(&m_version_data);

        if(NRF_SUCCESS == err_code) {
            init_finalize();
        } else if(NRF_ERROR_INVALID_STATE == err_code) {
            err_code == flash_config_delete();
            APP_ERROR_CHECK(err_code);
        } // else the WRITE or UPDATE events will be triggered    
        break;

    case FDS_EVT_WRITE: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "FDS event: WRITE\r\n");
        if(FDS_SUCCESS == p_evt->result && APP_CONFIG_VERSION_REC_KEY == p_evt->write.record_key) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Version FDS record created.\r\n");

            // If version info was written for the first time, remove config data just in case
            flash_config_delete();
        } else if(APP_CONFIG_CONFIG_REC_KEY == p_evt->write.record_key) {
            if(FDS_SUCCESS == p_evt->result) {
                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config FDS record created.\r\n");
                config_status_update(CONFIG_S_STATUS_SET);
            } else {
                config_status_update(CONFIG_S_STATUS_FAIL);
            }
        } else if(APP_CONFIG_PARAMS_REC_KEY == p_evt->write.record_key) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Params FDS record created.\r\n");
            handle_connection_abort();
        }
    } break;

    case FDS_EVT_UPDATE: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "FDS event: UPDATE\r\n");
        if (p_evt->result == FDS_SUCCESS && APP_CONFIG_VERSION_REC_KEY == p_evt->write.record_key) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Version FDS record updated.\r\n");
            // If version info was updated, continue with FDS operations
            if(APP_CONFIG_VERSION_REC_KEY == p_evt->write.record_key) {
                init_finalize();
            }
        } else if(APP_CONFIG_PARAMS_REC_KEY == p_evt->write.record_key) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Params FDS record updated.\r\n");
            handle_connection_abort();
        }
    } break;

    case FDS_EVT_DEL_RECORD: {
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "FDS event: DEL_RECORD\r\n");
        if (p_evt->result == FDS_SUCCESS) {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "FDS config data cleared.\r\n");
            app_timer_stop_all();
            blesc_toggle_leds(0, 0);
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "====== System restart ======\r\n");
            nrf_delay_ms(100);
            sd_nvic_SystemReset();
        }
    } break;

    default:
        __LOG(LOG_SRC_APP, LOG_LEVEL_DBG2, "FDS event: unhandled\r\n");
        break;
    }
}

void flash_pages_erase(void) {
    ret_code_t err_code = NRF_SUCCESS;
    uint32_t const bootloader_addr = BOOTLOADER_ADDRESS;
    uint32_t const page_sz         = NRF_FICR->CODEPAGESIZE;
    uint32_t const code_sz         = NRF_FICR->CODESIZE;
    uint32_t end_addr = (bootloader_addr != 0xFFFFFFFF) ? bootloader_addr : (code_sz * page_sz);
    end_addr = end_addr - (FDS_PHY_PAGES_RESERVED * FDS_PHY_PAGE_SIZE * sizeof(uint32_t));

    uint32_t cur_page = 0;

    for (uint32_t i = 1; i <= FDS_PHY_PAGES - FDS_PHY_PAGES_RESERVED; ++i) {
        cur_page = (end_addr / (sizeof(uint32_t) * FDS_PHY_PAGE_SIZE)) - i;
        do {
            err_code = sd_flash_page_erase(cur_page);
        } while(NRF_ERROR_BUSY == err_code);
        APP_ERROR_CHECK(err_code);
    }
}

void flash_init(void (*cb)(void)) {
    init_finalize = cb;

    /* Register first to receive an event when initialization is complete. */
    (void) fds_register(fds_evt_handler);

    ret_code_t err_code = fds_init();
    if(FDS_ERR_NO_PAGES == err_code) {
        flash_pages_erase();
        err_code = fds_init();
    }
    APP_ERROR_CHECK(err_code);
}

/** @}*/