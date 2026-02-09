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
    // Define the time to wait for temperature and pgood signal
    static constexpr uint32_t TEMP_PGODD_MONITOR_PERIOD_MS = 2000; // 2000 Milliseconds (adjust as needed)

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

        // IPG Temperature thresholds in Celsius
        static constexpr int8_t IPG_TEMP_THRESHOLD_HIGH = 41;   // Critical temperature in C
        static constexpr int8_t IPG_TEMP_THRESHOLD_MEDIUM = 39; // Warning temperature in C
        static constexpr int8_t IPG_TEMP_THRESHOLD_LOW = 36;    // Normal operating temperature in C

        // Enum for power adjustment state machine
        enum class PgoodState : uint8_t
        {
            INIT,             // Initial state
            INCREASING_POWER, // Incrementally increasing power
            STABILIZING,      // Found initial PGOOD=1, stabilizing
            FINE_TUNING,      // Fine-tuning power for stability
            STABLE            // Stable operation achieved
        };

        // Define constants
        static constexpr uint8_t MIN_POWER_LEVEL = 0;
        static constexpr uint8_t STABILITY_THRESHOLD = 10; // Number of consecutive stable readings
        static constexpr uint8_t MAX_FINE_TUNE_STEPS = 2;  // Maximum additional steps for fine tuning
        static constexpr uint8_t MAX_COUNT_TOGGLING = 3;   // Maximum toggle counting after stable PGOOG

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

        // Callback function for the status of temperature and pgood signal
        ///
        /// @param xTimer Handle to the timer
        static void StatusTempPgoodMonitoring(TimerHandle_t xTimer);

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

        /// Processes the new themal parameters get through BLE data
        static void IpgTemperatureMonitoring(void);

        /// Processes the new pgood parameter get through BLE data
        static void IpgPgoodMonitoring(void);

        /// Reset all PGOOD state machine variables to their default values
        static void ResetPgoodMonitoringStateMachine();

        // Sets the power level for wireless power transmission
        //
        // @param level The power level to set (0 to MAXIMUM)
        static void SetPowerLevel(uint8_t level);

        static eda::Timer mStatusTimer;

        static eda::Timer mStatusTimeoutTimer;

        static eda::Timer mTempPgoodTimer;

        hal::Dac80504 DacHalInstance;

        hal::Wpt_LTC4125 WptHalInstance;

        bool static m_is_high_temperature_threshold_exceeded;

        static PgoodState pgood_st_machine_current_state;
        static uint8_t pgood_st_machine_current_power_level;
        static uint8_t pgood_st_machine_stable_power_level;
        static uint8_t pgood_st_machine_unstability_toggle_count;
        static uint8_t pgood_st_machine_stability_counter;
        static uint8_t pgood_st_machine_fine_tune_attempts;
        static bool pgood_st_machine_last_pgood_status;
    };
}

#endif // SVC_WPT_MANAGER_H