/**
 * @name Hornet / WPT Charger
 * @file svc_debug_config.h
 * @brief Build-time configuration for the manual/debug control console
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SVC_DEBUG_CONFIG_H
#define SVC_DEBUG_CONFIG_H

/// Master switch for the RTT manual-control console.
///
/// 0 = production. No console, no poll timer, and every manual-mode gate in the
///     WPT and application layers is preprocessed away, so the binary is byte
///     identical to a build without this feature.
/// 1 = bench/debug build. The console is compiled in, but the firmware still
///     boots and runs its normal automatic control loop until the operator
///     presses 'm' over RTT.
#define WPT_MANUAL_DEBUG_MODE 0

/// How often the console checks the RTT down-channel for a keystroke.
#define WPT_MANUAL_POLL_PERIOD_MS 100

/// Dead-man timeout: while in manual mode, the coil is paused after this long
/// with no keystroke. Any key resets it. Set to 0 to disable.
///
/// This exists because manual mode deliberately removes every automatic cutoff:
/// the IPG still protects itself at 42 C, but a bench rig with no IPG (or a
/// walked-away-from session) has nothing watching the coil at all.
#define WPT_MANUAL_DEADMAN_MS 600000 // 10 minutes

#endif // SVC_DEBUG_CONFIG_H
