/**
 * @name Hornet / WPT Charger
 * @file hal_dfu.cpp
 * @brief DFU Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_dfu.h"

#include "eda_manager_log_config.h"

namespace hal
{
    Dfu &Dfu::Instance()
    {
        static Dfu instance;
        return instance;
    }

    Dfu::Dfu() : m_is_dfu_active(false)
    {
        // Constructor implementation
    }

    bool Dfu::is_dfu_active()
    {
        return m_is_dfu_active;
    }

    bool Dfu::start_dfu_mode()
    {
        ret_code_t err_code;
        LOG_WARNING("Start DFU process, the system will reset\r\n");

        // Clear GPREGRET
        err_code = sd_power_gpregret_clr(0, 0xffffffff);
        if (err_code != NRF_SUCCESS)
        {
            LOG_ERROR("Failed to clear GPREGRET: %d\r\n", err_code);
            return false;
        }

        // Set bootloader flag
        // Bootloader will check this GPREGRET register on startup to
        // determine if it should enter DFU mode or not
        err_code = sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
        if (err_code != NRF_SUCCESS)
        {
            LOG_ERROR("Failed to set DFU flag: %d\r\n", err_code);
            return false;
        }

        m_is_dfu_active = true;

        // Reset system to enter bootloader
        LOG_WARNING("System reset for DFU mode\r\n");
        NVIC_SystemReset();

        // Code will never reach here due to system reset
        return true;
    }
}
