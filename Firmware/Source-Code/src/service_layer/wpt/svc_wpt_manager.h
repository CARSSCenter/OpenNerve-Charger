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
            PAUSE_OVP = 1 << 1
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

        /// Stops the IPG temp and PGOOD status timer.
        void StopIpgTemperaturePgoodMonitoringTimer();

        /// Begins the open-loop cold-start attempt: drives COLD_START_LEVEL now and
        /// escalates to maximum power after COLD_START_ESCALATE_MS if no IPG
        /// advertisement has been seen by then.
        void StartColdStartEscalation();

        /// Set Wpt power Transfer pulse width
        void AdjustWptPowerTransfer(uint8_t step);

        int16_t mWptImonVoltage;

        int16_t mWptNtcVoltage;

        int16_t mWptNtcTemperature;

    private:
        static constexpr uint32_t k_resistor_value = 49900; // 49.9kΩ in ohms

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
        static constexpr uint32_t COLD_START_ESCALATE_MS = 5000;

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
    };
}

#endif // SVC_WPT_MANAGER_H