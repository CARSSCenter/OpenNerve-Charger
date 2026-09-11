/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_manager.h
 * @brief WptManager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SVC_WPT_MANAGER_H
#define SVC_WPT_MANAGER_H

#include "svc_wpt_port.h"

#include "hal_gpio.h"
#include "hal_pinout.h"
#include "hal_wpt.h"
#include "hal_dac.h"
#include "eda_timer.h"
#include "svc_debug_config.h"

namespace svc
{
    // Fault monitoring (IPG temperature + OVP) runs fast: the IPG only holds itself
    // in its PAUSED state for WPT_OVP_PAUSE_HOLD_MS (5 s) after a VRECT_OVP event
    // before re-enabling, so the charger has to see and react inside that window.
    static constexpr uint32_t FAULT_MONITOR_PERIOD_MS = 2000; // 2 s

    // Power control runs slow: a PTH change needs time to settle and then has to
    // propagate back through an IPG BLE advertisement before it can be evaluated.
    static constexpr uint32_t POWER_CTRL_PERIOD_MS = 10000; // 10 s

    class WptManager
    {
    public:
        /// Returns the WPT manager instance.
        static WptManager &Instance();

        /// Initializes the WPT manager.
        void Init();

        /// Enables the WPT.
        void EnableWpt();

        /// Disables the WPT.
        void DisableWpt();

        /// Reasons the coil output can be held off. The coil is enabled only when
        /// no reason is set, so overlapping faults cannot resume each other's pause.
        enum PauseReason_e : uint8_t
        {
            PAUSE_THERMAL = 1 << 0,
            PAUSE_OVP = 1 << 1,
            /// Set only by the RTT debug console. Uses the same mask as the fault
            /// reasons so a manual stop cannot be undone by an automatic resume,
            /// and so the coil-enabled-iff-mask-is-zero rule keeps holding.
            PAUSE_MANUAL = 1 << 2
        };

        /// Suspends coil output for a fault condition, without stopping fault or
        /// power monitoring, so the recovery condition can still be detected.
        ///
        /// @param reason One of PauseReason_e
        void PauseWpt(uint8_t reason);

        /// Clears one pause reason. The coil is re-enabled only once every reason
        /// has been cleared.
        ///
        /// @param reason One of PauseReason_e
        void ResumeWpt(uint8_t reason);

        /// Stops the WPT scan.
        void StopWptScan();

        /// Retrieves the current from the Imon pin.
        ///
        /// @param callback The callback function
        void GetCurrent(hal::MeasurementReadyCallback_t callback);

        /// Retrieves the temperature from the NTC pin.
        ///
        void GetTemperature();

        /// Periodically retrieves the charge status.
        void StartStatusMonitoring();

        /// Stops the status monitoring.
        void StopStatusMonitoring();

        /// Starts the status timeout timer for handling disconnection events.
        void StartStatusTimeoutTimer();

        /// Stops the status timeout timer.
        void StopStatusTimeoutTimer();

        /// Starts the IPG temp and PGOOD status timer.
        void StartIpgTemperaturePgoodMonitoringTimer();

        /// Starts fault monitoring (IPG temperature + OVP) WITHOUT the power
        /// control timer.
        ///
        /// Used by the manual bench state: the fault monitor must run so the
        /// thermal and OVP thresholds are still evaluated and reported, but the
        /// titration loop must never move the operator's level. Not starting the
        /// timer is what makes that structural rather than a runtime check.
        /// StopIpgTemperaturePgoodMonitoringTimer() still stops both, so there is
        /// no matching partial stop.
        void StartFaultMonitoringOnly();

#if WPT_MANUAL_DEBUG_MODE
        /// Establishes manual mode's idle baseline: coil off, every WPT timer
        /// stopped, any pending cold-start escalation cancelled, power search reset,
        /// then fault monitoring restarted on its own.
        ///
        /// Must run on the WPT task, dispatched from WPT_MANUAL_IDLE. The SYSTEM task
        /// outranks the WPT task, so anything the previous application state queued
        /// here is processed after StateManual::Entry(); only an event queued behind
        /// those can be relied on to have the last word.
        void EnterManualIdle();
#endif

        /// Stops the IPG temp and PGOOD status timer.
        void StopIpgTemperaturePgoodMonitoringTimer();

        /// Begins the open-loop cold-start attempt: drives COLD_START_LEVEL now and
        /// escalates to maximum power after COLD_START_ESCALATE_MS if no IPG
        /// advertisement has been seen by then.
        void StartColdStartEscalation();

        /// Set Wpt power Transfer pulse width
        void AdjustWptPowerTransfer(uint8_t step);

        /// Applies an absolute PTH step on behalf of the RTT debug console.
        ///
        /// Goes through the same path as the automatic loop so that m_level stays
        /// the single source of truth for the current step - setting the DAC alone
        /// would leave the recorded level stale and desynchronise the status dump
        /// and any later automatic resume.
        ///
        /// @param level PTH step; clamped to the hardware maximum.
        static void SetPowerLevelManual(uint8_t level);

        /// Current PTH step.
        static uint8_t GetPowerLevel() { return m_level; }

        /// Highest PTH step the hardware accepts.
        static uint8_t GetMaxPowerLevel();

        /// Bitmask of PauseReason_e. Zero means the coil is enabled.
        static uint8_t GetPauseReasons() { return m_pause_reasons; }

        /// Lowest step observed to trip an IPG OVP fault, or LEVEL_INVALID.
        static uint8_t GetOvpCeiling() { return m_ovp_ceiling; }

        /// Highest step observed to be insufficient for PGOOD, or LEVEL_INVALID.
        static uint8_t GetPgoodFloor() { return m_pgood_floor; }

        /// True once the downward power search has settled.
        static bool IsFloorFound() { return m_floor_found; }

        /// True while the coil is actually being driven.
        ///
        /// Not the same as GetPauseReasons() == 0: with the WPT state machine in
        /// StateIdle the mask is zero and the coil is off, which is exactly where
        /// manual mode idles before 's' is pressed.
        static bool IsCoilEnabled() { return m_coil_enabled; }

        /// Re-opens the downward power search without discarding what has been
        /// observed about the usable window.
        ///
        /// Needed when handing control back from the debug console: the level the
        /// operator happened to stop on is not a floor the loop derived, but
        /// m_floor_found may still be set from before. Leaving it set would make
        /// the loop hold that level indefinitely instead of resuming its descent.
        /// m_ovp_ceiling and m_pgood_floor are deliberately kept - those are real
        /// observations of the hardware and stay valid.
        static void RearmPowerSearch();

        int16_t mWptImonVoltage;

        int16_t mWptNtcVoltage;

        int16_t mWptNtcTemperature;

    private:
        static constexpr uint32_t k_resistor_value = 49900; // 49.9kΩ in ohms

        // Largest value a wrapped THERM_OUT byte can decode to. The IPG packs OUT
        // unclamped as (mV / 10) in one byte, so it wraps at 2560 mV, and its ADC
        // cannot read above VREF+ <= 3.6 V (STM32 supply maximum):
        // 3600 - 2560 = 1040 mV. See IsThermReadingPlausible().
        static constexpr uint16_t THERM_OUT_WRAP_MAX_MV = 1040;

        // Temperature lookup table entry
        struct TempResistancePair
        {
            int8_t temp;         // Temperature in °C
            uint32_t resistance; // resistance in kΩ
        };

        // Lookup table for temperature-resistance conversion
        // This table maps thermistor resistance (in kΩ) to corresponding temperatures (in °C)
        // The part nuember is 104AP-2 thermistor
        // Ordered from lowest temperature to highest temperature
        static constexpr TempResistancePair TEMP_LOOKUP_TABLE[] = {
            // Low to mid-range temperatures
            {20, 126400}, // At 20°C, resistance is 126.4 KΩ
            {25, 100000}, // Reference point at 25°C (typical room temperature), resistance is 100 Ω

            // Mid to high temperatures
            {30, 79590}, // At 30°C, resistance is 79.59 KΩ
            {40, 51320}, // At 40°C, resistance is 51.29 KΩ
            {50, 33790}, // At 50°C, resistance is 33.79 KΩ
        };

        static constexpr size_t LOOKUP_TABLE_SIZE = sizeof(TEMP_LOOKUP_TABLE) / sizeof(TempResistancePair);

        // IPG Temperature thresholds in Celsius (single hysteresis band)
        static constexpr int8_t IPG_TEMP_THRESHOLD_PAUSE = 41;  // Pause power transfer at/above this
        static constexpr int8_t IPG_TEMP_THRESHOLD_RESUME = 39; // Resume power transfer at/below this

        // Minimum time held in a thermal pause before resume is even considered,
        // counted in FAULT_MONITOR_PERIOD_MS ticks. 15 * 2 s = 30 s.
        static constexpr uint16_t THERMAL_PAUSE_MIN_TICKS = 15;

        // Minimum time held in an OVP pause, in fault-monitor ticks. 5 * 2 s = 10 s.
        static constexpr uint16_t OVP_PAUSE_MIN_TICKS = 5;

        // Power levels are PTH steps into the DAC: 0 = 400 mV, 12 = 1600 mV.
        static constexpr uint8_t MIN_POWER_LEVEL = 0;

        // Sentinel for "no bound observed yet". Not a reachable power level.
        static constexpr uint8_t LEVEL_INVALID = 0xFF;

        // Where the closed loop starts once BLE telemetry is available, and where
        // the open-loop cold-start attempt begins. Mid-range: high enough to wake a
        // drained IPG, low enough not to drive VRECT straight into OVP.
        static constexpr uint8_t COLD_START_LEVEL = 7;

        // Passed to SetPulseWidthThresholdStep() to request maximum power. Out of
        // range on purpose - the HAL clamps anything above the maximum step to
        // VoltageMaxPulseWidthThreshold_mV (hal_wpt.cpp), so this cannot overshoot.
        static constexpr uint8_t LEVEL_REQUEST_MAX = 0xFE;

        // Consecutive PGOOD=0 control cycles required before stepping power up.
        // Rejects single-sample dropouts without delaying a real insufficiency by
        // more than one cycle.
        static constexpr uint8_t PGOOD_LOW_CONFIRM = 2;

        // Control cycles skipped after a fault resume. When the IPG comes back from
        // its own PAUSED state, VCHG needs a moment to recover, so PGOOD reads 0
        // with OVP already cleared - which is exactly the "step up" condition and
        // would drive us straight back into the fault.
        static constexpr uint8_t BLANK_CYCLES_AFTER_FAULT = 1;

        // How long the cold-start attempt stays at COLD_START_LEVEL before
        // escalating to maximum power for the rest of the scan window.
        static constexpr uint32_t COLD_START_ESCALATE_MS = 30000;

        // Cold start: true  = if no IPG advertisement after COLD_START_ESCALATE_MS at
        // COLD_START_LEVEL, jump to maximum power.
        //false = stay at COLD_START_LEVEL for the whole white-light window.
        static constexpr bool COLD_START_ESCALATION_ENABLED = false;

        /// Construct WptManager
        WptManager();

        /// Retrieves the status from the Stat pin.
        static void GetChargeStatus(uint32_t *status);

        /// Callback function for the status timer
        ///
        /// @param xTimer Handle to the timer
        static void StatusMonitoring(TimerHandle_t xTimer);

        /// Callback function for the status timeout timer
        ///
        /// @param xTimer Handle to the timer
        static void StatusTimeoutMonitoring(TimerHandle_t xTimer);

        /// Callback for the fast fault timer: samples IPG temperature and OVP.
        ///
        /// @param xTimer Handle to the timer
        static void FaultMonitoring(TimerHandle_t xTimer);

        /// Callback for the slow power control timer: runs the search for the
        /// minimum viable power level.
        ///
        /// @param xTimer Handle to the timer
        static void PowerControlMonitoring(TimerHandle_t xTimer);

        /// Callback for the cold-start escalation timer: raises power to maximum
        /// if no IPG advertisement has been seen yet.
        ///
        /// @param xTimer Handle to the timer
        static void ColdStartEscalate(TimerHandle_t xTimer);

        /// This method configures the GPIOs.
        void ConfigureGpios();

        /// Calculate temperature from resistance
        ///
        /// @param get_therm_ref Reference voltage from BLE advertisement
        /// @param get_therm_out Output voltage from BLE advertisement
        /// @param get_therm_ofst Offset voltage from BLE advertisement
        /// @return Calculated temperature in degrees Celsius
        static float CalculateTemperatureFromBle(uint16_t get_therm_ref,
                                                 uint16_t get_therm_out,
                                                 uint16_t get_therm_ofst);

        /// Rejects thermistor readings no real thermistor can produce - chiefly
        /// the IPG's unclamped OUT byte wrapping to ~0 above 2.56 V ("n/a").
        ///
        /// @return true if the reading may be passed to CalculateTemperatureFromBle()
        static bool IsThermReadingPlausible(uint16_t get_therm_ref,
                                            uint16_t get_therm_out,
                                            uint16_t get_therm_ofst);

        /// Evaluates the IPG temperature from BLE data and drives the thermal
        /// pause/resume hysteresis.
        static void IpgTemperatureMonitoring(void);

        /// Evaluates the IPG OVP flags from BLE data and drives the OVP
        /// pause/back-off handling.
        static void IpgOvpMonitoring(void);

        /// Resets the power search back to its initial state.
        static void ResetPowerControl();

        /// True when the observed OVP ceiling sits at or below the observed PGOOD
        /// floor, i.e. no power level can satisfy the IPG without faulting it.
        static bool IsPowerWindowEmpty();

        /// Charges a rectifier OVP fault to a power level, keeping the lowest
        /// level that has ever faulted as m_ovp_ceiling.
        static void RecordOvpCeiling(uint8_t level);

        /// Applies a power level and records it as the current level.
        ///
        /// @param level The power level to set (MIN_POWER_LEVEL to maximum step)
        static void SetPowerLevel(uint8_t level);

        static eda::Timer mStatusTimer;

        static eda::Timer mStatusTimeoutTimer;

        static eda::Timer mFaultTimer;

        static eda::Timer mPowerCtrlTimer;

        static eda::Timer mColdStartEscalateTimer;

        hal::Dac80504 DacHalInstance;

        hal::Wpt_LTC4125 WptHalInstance;

        // Bitmask of PauseReason_e. Coil output is enabled only while this is zero.
        static uint8_t m_pause_reasons;

        // Tracks the actual coil drive state, which the pause mask alone cannot
        // express - see IsCoilEnabled().
        static bool m_coil_enabled;

        // Time held in each pause, counted in fault-monitor ticks.
        static uint16_t m_thermal_pause_ticks;
        static uint16_t m_ovp_pause_ticks;

        // Current PTH step.
        static uint8_t m_level;

        // Set once the descent has found the lowest level the IPG still accepts.
        // From then on the level only ratchets up, until a thermal pause re-arms
        // the search.
        static bool m_floor_found;

        // Lowest level observed to trip an IPG OVP fault, and highest level
        // observed to be insufficient for PGOOD. LEVEL_INVALID until seen.
        static uint8_t m_ovp_ceiling;
        static uint8_t m_pgood_floor;

        // Control cycles still to be skipped after a fault resume.
        static uint8_t m_blank_cycles;

        // Consecutive PGOOD=0 control cycles observed.
        static uint8_t m_pgood_low_count;

        // Advertisement counter seen at the last control cycle, used to skip a
        // cycle when no fresh IPG telemetry has arrived since the last decision.
        static uint32_t m_last_adv_count;

        // False until the first control cycle with real BLE data, which forces the
        // level to COLD_START_LEVEL regardless of where cold start left it.
        static bool m_loop_initialized;

        // Advertisement counter seen at the last OVP evaluation; a fault is acted
        // on only when it arrives in a new advertisement. Deliberately not reset by
        // ResetPowerControl(): the counter only grows, and keeping it means the
        // last advertisement of a previous session is never mistaken for fresh.
        static uint32_t m_ovp_last_adv_count;

        // Last CHG1/CHG2_OVP_ERRn state that was logged (true = asserted). The
        // battery OVP flags are log-only, so they are reported on change rather
        // than on every advertisement.
        static bool m_chg1_ovp_logged;
        static bool m_chg2_ovp_logged;

        // Highest level applied during the current and the previous fault tick.
        // IPG telemetry lags the level by up to ~3 s (1 s IPG sampling plus the 2 s
        // fault tick), so a rectifier fault is charged to the highest level applied
        // over that window, not to whatever level is in force when it is read.
        // Otherwise a step down taken just before the fault is read - by the power
        // loop, the cold-start handover or a manual '-' - pins the ceiling on a
        // level that never faulted. Erring high is self-correcting (the level just
        // below re-trips and lowers the ceiling); erring low is not, because the
        // ceiling never rises again within a session.
        static uint8_t m_level_max_this_tick;
        static uint8_t m_level_max_last_tick;
    };
}

#endif // SVC_WPT_MANAGER_H