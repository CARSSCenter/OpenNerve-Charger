#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include <nrf_log.h>

#define LOG_ENABLED 1
#if LOG_ENABLED
    #define LOG_ERROR(...)                 NRF_LOG_ERROR(__VA_ARGS__)
    #define LOG_WARNING(...)               NRF_LOG_WARNING( __VA_ARGS__)
    #define LOG_INFO(...)                  NRF_LOG_INFO( __VA_ARGS__)
    #define LOG_DEBUG(...)                 NRF_LOG_DEBUG( __VA_ARGS__)
    #define LOG_FLUSH()                    NRF_LOG_FLUSH()
#else
    #define LOG_ERROR(...)
    #define LOG_WARNING(...)
    #define LOG_INFO(...)
    #define LOG_DEBUG(...)
    #define LOG_FLUSH()  
#endif

#endif // LOG_CONFIG_H