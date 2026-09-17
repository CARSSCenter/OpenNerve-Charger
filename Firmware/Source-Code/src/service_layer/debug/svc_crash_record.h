/**
 * @name Hornet / WPT Charger
 * @file svc_crash_record.h
 * @brief Post-mortem fault capture in no-init RAM, reported on the next boot
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SVC_CRASH_RECORD_H
#define SVC_CRASH_RECORD_H

#include <FreeRTOS.h>
#include <task.h>

#include <cstdint>

namespace svc
{
    /// Replaces the SDK's "halt forever" fault behaviour with "record, then reset".
    ///
    /// The SDK's Debug-build handlers execute a breakpoint - which halts the core
    /// whenever a debugger is attached - and then spin in app_error_save_and_stop()
    /// with interrupts disabled. On this bench the J-Link is always attached for
    /// RTT logging, so every fault looked like dead hardware: logging stopped, the
    /// buttons did nothing (the reset button is a GPIOTE ISR, and a halted core
    /// serves no interrupts), and only a re-flash brought it back - because
    /// re-flashing is what resets the core.
    ///
    /// Every fault path now writes what it knows into a .non_init block and calls
    /// NVIC_SystemReset(). A soft reset keeps both the RAM contents and the debug
    /// connection, so ReportAtBoot() prints the record into the same RTT session a
    /// second later and the charger carries on running.
    class CrashRecord
    {
    public:
        /// What kind of fault was recorded.
        enum Type_e : uint32_t
        {
            TYPE_NONE = 0,
            TYPE_SDK_ERROR,      ///< APP_ERROR_CHECK / APP_ERROR_CHECK_BOOL
            TYPE_SDK_ASSERT,     ///< ASSERT, NRFX_ASSERT, FreeRTOS configASSERT
            TYPE_SD_ASSERT,      ///< SoftDevice assertion
            TYPE_APP_MEMACC,     ///< SoftDevice caught an invalid memory access
            TYPE_UNKNOWN_FAULT,  ///< fault id outside the ranges above
            TYPE_HARD_FAULT,     ///< CPU HardFault
            TYPE_STACK_OVERFLOW, ///< FreeRTOS stack check
        };

        /// Logs the reset reason and any stored crash record, then paints the
        /// unused main stack so LogStackHeadroom() can measure it.
        ///
        /// Call once from System::Init(), after the log backend is up and before
        /// the SoftDevice is enabled: RESETREAS is read and cleared directly,
        /// which is only permitted while the SoftDevice is disabled.
        static void ReportAtBoot();

        /// Remembers how far the heartbeat had got, so a crash report can say how
        /// long the firmware had been running. Called from the heartbeat timer.
        static void NoteHeartbeat(uint32_t heartbeat);

        /// Logs the unused stack for the main (ISR) stack and for every task.
        /// Stack overflow is otherwise silent, and the 2 kB main stack also
        /// carries SoftDevice event dispatch, which runs in interrupt context.
        static void LogStackHeadroom();
    };
}

#endif // SVC_CRASH_RECORD_H
