#ifndef BLESC_LOG_SETTINGS_H__
#define BLESC_LOG_SETTINGS_H__

#include <stdbool.h>

#ifdef BOARD_IBKS_PLUS
  #define BLESC_LOG_ENABLED 0
#else
  #define BLESC_LOG_ENABLED 1
#endif

/** Enable logging module. */
#ifndef NRF_MESH_LOG_ENABLE
#define NRF_MESH_LOG_ENABLE BLESC_LOG_ENABLED
#endif

/** Default log level. Messages with lower criticality is filtered. */
#ifndef LOG_LEVEL_DEFAULT
#define LOG_LEVEL_DEFAULT LOG_LEVEL_WARN
#endif

/** Default log mask. Messages with other sources are filtered. */
#ifndef LOG_MSK_DEFAULT
#define LOG_MSK_DEFAULT LOG_GROUP_STACK
#endif

/** Enable logging with RTT callback. */
#ifndef LOG_ENABLE_RTT
#define LOG_ENABLE_RTT BLESC_LOG_ENABLED
#endif

/** The default callback function to use. */
#ifndef LOG_CALLBACK_DEFAULT
#if defined(NRF51) || defined(NRF52_SERIES)
    #define LOG_CALLBACK_DEFAULT log_callback_rtt
#else
    #define LOG_CALLBACK_DEFAULT log_callback_stdout
#endif
#endif


#endif /* BLESC_LOG_SETTINGS_H__ */