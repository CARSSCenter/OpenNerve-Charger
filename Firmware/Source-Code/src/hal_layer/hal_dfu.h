/**
 * @name Hornet / WPT Charger
 * @file hal_dfu.h
 * @brief DFU Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_DFU_HPP
#define HAL_DFU_HPP

#include <stdint.h>

#include "app_error.h"
#include "nrf_bootloader_info.h"
#include "nrf_dfu_types.h"
#include "nrf_dfu_utils.h"
#include "sdk_macros.h"

namespace hal
{
    class Dfu
    {
    public:
        /// Get the singleton instance of Dfu
        ///
        /// @return Reference to Dfu instance
        static Dfu &Instance();

        /// Check if DFU mode is currently active
        ///
        /// @return true if DFU is active, false otherwise
        bool is_dfu_active();

        /// Enter DFU mode by setting bootloader flag and resetting
        ///
        /// @return true if operation was successful, false otherwise
        bool start_dfu_mode();

    private:
        Dfu();
        ~Dfu() = default;
        Dfu(const Dfu &) = delete;            // Prevent copy construction
        Dfu &operator=(const Dfu &) = delete; // Prevent assignment

        bool m_is_dfu_active; // Flag to track DFU state
    };
}

#endif // HAL_DFU_HPP