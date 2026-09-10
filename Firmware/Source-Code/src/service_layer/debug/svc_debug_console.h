/**
 * @name Hornet / WPT Charger
 * @file svc_debug_console.h
 * @brief Manual power-control console over the SEGGER RTT down-channel
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SVC_DEBUG_CONSOLE_H
#define SVC_DEBUG_CONSOLE_H

#include "svc_debug_config.h"

#include <cstdint>

#if WPT_MANUAL_DEBUG_MODE
#include "eda_timer.h"
#endif

namespace svc
{
    /// Bench-test console: lets an operator drive the coil by hand over RTT while
    /// the normal logging continues.
    ///
    /// In manual mode the thermal and OVP monitors still run and still evaluate
    /// their thresholds, but they only warn - they never pause the coil or move
    /// the power level. The automatic titration loop, the cold-start escalation
    /// and the application-layer transitions that would stop a charge session are
    /// all inhibited, so the commanded level is the level that stays applied.
    ///
    /// Note that this cannot make the system as a whole unprotected: the IPG runs
    /// its own independent thermal gate and OVP shutdown, which the charger has no
    /// way to disable. That is precisely why manual mode logs the IPG's own fault
    /// bits every fault tick - so a self-protecting IPG is never mistaken for a
    /// command that failed to take effect.
    class DebugConsole
    {
    public:
#if WPT_MANUAL_DEBUG_MODE

        /// Creates and starts the RTT poll timer. Call after the log backend is up.
        static void Init();

        /// The single predicate every manual-mode gate checks.
        static bool IsManual() { return m_manual; }

        /// Logs the IPG's raw fault bits. Called once per fault tick while in
        /// manual mode so charger-side and IPG-side causes stay distinguishable.
        static void LogIpgBits();

        /// Called only by StateManual::Entry() and Exit(). m_manual is owned by the
        /// state, so the console can never believe it is in manual mode while the
        /// application state machine is somewhere else.
        static void OnManualEntered();
        static void OnManualExited();

    private:
        /// Timer callback: drains the RTT down-channel and ages the dead-man timer.
        static void Poll(TimerHandle_t xTimer);

        /// Acts on one keystroke.
        static void HandleKey(char key);

        static void EnterManual();
        static void ExitManual();

        /// Applies an absolute PTH step, clamped to the hardware maximum.
        static void SetLevel(uint8_t level);

        /// Applies a relative PTH step, saturating at both ends.
        static void StepLevel(int8_t delta);

        static void StartCoil();
        static void StopCoil();
        static void PrintStatus();
        static void PrintHelp();

        static eda::Timer mPollTimer;

        /// True exactly while the application state machine is in StateManual.
        /// Written only by OnManualEntered()/OnManualExited().
        static bool m_manual;

        /// Milliseconds since the last keystroke, for the dead-man timeout.
        static uint32_t m_idle_ms;

        /// Latches when the dead-man timeout has fired, so it pauses the coil once
        /// rather than re-issuing the event every poll.
        static bool m_deadman_tripped;

#else

        /// Production build: no console, and every gate compiles to nothing.
        static void Init() {}

#endif // WPT_MANUAL_DEBUG_MODE
    };
}

#endif // SVC_DEBUG_CONSOLE_H
