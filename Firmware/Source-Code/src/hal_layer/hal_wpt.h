/**
 * @name Hornet / WPT Charger
 * @file hal_wpt.h
 * @brief WPT Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_WPT_H
#define HAL_WPT_H

#include "hal_adc.h"
#include "hal_dac.h"
#include "hal_timer.h"

const int16_t NTC_R0 = 5000;   // NTC nominal resistance at 25°C
const int16_t NTC_BETA = 3480; // NTC Beta coefficient
const int32_t T0_mK = 298150;  // 25°C in milliKelvin (298.15K)

namespace hal
{
    using MeasurementReadyCallback_t = void (*)(const uint16_t);

    class Wpt_LTC4125
    {
    public:
        /// Constructor for the WPT module
        Wpt_LTC4125();

        /// Initialize the WPT module by setting the DAC values
        void Init(void);

        /// Enable the WPT module
        void Enable(void);

        /// Disable the WPT module
        void Disable(void);

        /// Stop the WPT power search
        void StopSearch(void);

        /// Restart the WPT power search
        void ResumeSearch(void);

        /// Get the NTC value
        ///
        void GetNtc();

        /// Get the IMON current value
        ///
        /// @param callback The callback function
        void GetImon(MeasurementReadyCallback_t callback);

        /// Get the Status of the WPT module
        static uint8_t GetStat(void);

        /// Set pulse width threshold in mV steps
        void SetPulseWidthThresholdStep(uint8_t step);

        uint8_t GetMaxPulseWidthThresholdStep(void);

    private:
        Dac80504 mDac;

        /// Start the IMON measurement
        void StartMeasureImon(void);

        /// Stop the IMON measurement
        void StopMeasureImon(void);

        /// Start the NTC measurement
        void StartMeasureNtc(void);

        /// Convert the NTC voltage to temperature in ºC
        int16_t NtcVoltageToTemperature(int32_t Vout_mV);

        /// Stop the NTC measurement
        void StopMeasureNtc(void);

        /// Set the delta FB threshold
        void SetDeltaFBThreshold(void);

        /// Set the frequency threshold
        void SetFrequencyThreshold(void);

        /// Set the pulse width threshold in mV
        void SetPulseWidthThreshold(uint16_t voltage_mV);

        /// Set the minimun driver pulse width
        void SetMinimunDriverPulseWidth(void);

        /// Configure the DAC
        void ConfigureDac(void);

        /// Callback function for the IMON measurement
        ///
        /// @param context The context
        static void wptImonCallback(const void *context);

        /// Callback function for the NTC measurement
        ///
        /// @param context The context
        static void wptNtcCallback(const void *context);

        static MeasurementReadyCallback_t mImonReadyCallback;

        static MeasurementReadyCallback_t mNtcReadyCallback;

        static constexpr uint8_t AdcBufferSize = 16;

        static constexpr uint32_t PeriodTimer = 78126;

        static constexpr uint16_t VoltageDeltaFBThreshold = 1500;

        static constexpr uint16_t VoltageFrequencyThreshold = 0;

        static constexpr uint16_t VoltageMinPulseWidthThreshold_mV = 400;  // 400mV
        static constexpr uint16_t VoltageMaxPulseWidthThreshold_mV = 1600; // 1600mV
        static constexpr uint16_t VoltageStepPulseWidthThreshold_mV = 100; // 100mV per step

        // Calculate the maximum step
        static constexpr uint8_t MAX_VOLTAGE_STEP_PULSE_WIDTH =
            ((VoltageMaxPulseWidthThreshold_mV - VoltageMinPulseWidthThreshold_mV) / VoltageStepPulseWidthThreshold_mV);

        static constexpr uint16_t VoltageMinimunDriverPulseWidth = 100;

        int16_t mNtc[AdcBufferSize] = {0};

        int16_t mImon[AdcBufferSize] = {0};

        int16_t mimonVoltage = 0;

        int16_t mNtcValue = 0;

        static constexpr Timer::Config_t mtimerImonConfig = {
            .period = PeriodTimer,
            .autostart = true,
            .timerNumber = Timer::PeripheralNumber::TIMER_1};

        static constexpr Timer::Config_t mtimerNtcConfig = {
            .period = PeriodTimer,
            .autostart = true,
            .timerNumber = Timer::PeripheralNumber::TIMER_2};

        Timer timerImon;
        Timer timerNtc;
    };
}
#endif