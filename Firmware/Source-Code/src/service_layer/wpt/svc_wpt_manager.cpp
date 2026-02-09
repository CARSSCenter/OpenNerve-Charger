/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_manager.cpp
 * @brief WptManager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_wpt_manager.h"

#include "app_system.h"
#include "eda_manager_log_config.h"
#include "hal_dac.h"
#include "svc_ble_subsystem.h"
#include "svc_wpt_subsystem.h"

namespace svc
{
    bool WptManager::m_is_high_temperature_threshold_exceeded = false;

    uint8_t static m_max_power_level;

    WptManager::PgoodState WptManager::pgood_st_machine_current_state;
    uint8_t WptManager::pgood_st_machine_current_power_level;
    uint8_t WptManager::pgood_st_machine_stable_power_level;
    uint8_t WptManager::pgood_st_machine_unstability_toggle_count;
    uint8_t WptManager::pgood_st_machine_stability_counter;
    uint8_t WptManager::pgood_st_machine_fine_tune_attempts;
    bool WptManager::pgood_st_machine_last_pgood_status;

    WptManager &WptManager::Instance()
    {
        static WptManager instance;
        return instance;
    }

    WptManager::WptManager() : WptHalInstance()
    {
    }

    eda::Timer WptManager::mStatusTimer("WptStatusTimer", 500, 1, StatusMonitoring);

    eda::Timer WptManager::mStatusTimeoutTimer("WptStatusTimeoutTimer", 5000, 0, StatusTimeoutMonitoring);

    eda::Timer WptManager::mTempPgoodTimer("WptStatusTempPgood", TEMP_PGODD_MONITOR_PERIOD_MS, 1, StatusTempPgoodMonitoring);

    void WptManager::Init()
    {
        LOG_DEBUG("WPT Manager: Init\n");
        ConfigureGpios();
        DacHalInstance.Init();
        WptHalInstance.Init();
        ResetPgoodMonitoringStateMachine();
        m_max_power_level = WptHalInstance.GetMaxPulseWidthThresholdStep();
    }

    void WptManager::ConfigureGpios()
    {
        LOG_DEBUG("WPT Manager: ConfigGpios\n");
        LOG_DEBUG("Call a hal_dac method that config gpios\n");
        LOG_DEBUG("Call a hal_wpt method that config gpios\n");

        // Configure WPT En Pin
        static constexpr hal::Gpio::gpio_config_t gpioWptEnConfig = {
            .pin_number = PIN_WPT_EN,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_OUTPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_PULLUP,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(gpioWptEnConfig);
        hal::Gpio::Write(PIN_WPT_EN, 1);

        // The following code is commented out because the CTD pin is not implemented in the prototype

        // Configure WPT CTD Pin
        /* static constexpr hal::Gpio::gpio_config_t gpioCtdConfig = {
            .pin_number = PIN_WPT_CTD,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_OUTPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_PULLDOWN,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(gpioCtdConfig);
        hal::Gpio::Write(PIN_WPT_CTD, 0); */

        // Configure WPT STAT Pin
        static constexpr hal::Gpio::gpio_config_t gpioStatConfig = {
            .pin_number = PIN_WPT_STAT,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_INPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_CONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_PULLUP,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(gpioStatConfig);
    }

    void WptManager::EnableWpt()
    {
        WptHalInstance.Enable();
        StartStatusTimeoutTimer();
        StartStatusMonitoring();
        LOG_DEBUG("WPT Manager: EnableWpt\n");
    }

    void WptManager::DisableWpt()
    {
        WptHalInstance.Disable();
        StopStatusMonitoring();
        ResetPgoodMonitoringStateMachine();
        StopIpgTemperaturePgoodMonitoringTimer();
        LOG_DEBUG("WPT Manager: DisableWpt\n");
    }

    void WptManager::StopWptScan()
    {
        WptHalInstance.StopSearch();
        LOG_DEBUG("WPT Manager: StopWptScan\n");
    }

    void WptManager::GetCurrent(hal::MeasurementReadyCallback_t callback)
    {
        WptHalInstance.GetImon(callback);
        LOG_DEBUG("WPT Manager: GetCurrent\n");
    }

    void WptManager::GetTemperature()
    {
        WptHalInstance.GetNtc();
        LOG_DEBUG("WPT Manager: GetTemperature\n");
    }

    void WptManager::GetChargeStatus(uint32_t *status)
    {
        *status = hal::Wpt_LTC4125::GetStat();
        LOG_DEBUG("WPT Manager: GetChargeStatus\n");
    }

    void WptManager::StartStatusTimeoutTimer()
    {
        LOG_DEBUG("WPT Manager: StartStatusTimeoutTimer\n");
        mStatusTimeoutTimer.Start();
    }

    void WptManager::StopStatusTimeoutTimer()
    {
        LOG_DEBUG("WPT Manager: StopStatusTimeoutTimer\n");
        mStatusTimeoutTimer.Stop();
    }

    void WptManager::StatusTimeoutMonitoring(TimerHandle_t xTimer)
    {
        LOG_DEBUG("WPT Manager: StatusTimeoutMonitoring\n");
        if (hal::Wpt_LTC4125::GetStat() == 1)
        {
            WptPort::SendEventFromISR(WptPort::Event_e::WPT_SCAN_TIMEOUT, NULL);
        }
    }

    void WptManager::StartStatusMonitoring()
    {
        LOG_DEBUG("WPT Manager: StartStatusMonitoring\n");
        mStatusTimer.Start();
    }

    void WptManager::StopStatusMonitoring()
    {
        LOG_DEBUG("WPT Manager: StopStatusMonitoring\n");
        mStatusTimer.Stop();
    }

    void WptManager::StatusMonitoring(TimerHandle_t xTimer)
    {
        static uint32_t status = 0;
        GetChargeStatus(&status);
        LOG_DEBUG("WPT Manager: Status pin value: %d \n", status);
    }

    void WptManager::StartIpgTemperaturePgoodMonitoringTimer()
    {
        LOG_DEBUG("WPT Manager: StartIpgTemperatureMonitoringTimer\n");
        mTempPgoodTimer.Start();
    }

    void WptManager::StopIpgTemperaturePgoodMonitoringTimer()
    {
        LOG_DEBUG("WPT Manager: StopIpgTemperatureMonitoringTimer\n");
        mTempPgoodTimer.Stop();
    }

    float WptManager::CalculateTemperatureFromBle(uint16_t get_therm_ref,
                                                  uint16_t get_therm_out,
                                                  uint16_t get_therm_ofst)
    {
        // Therm reference -> For 35-42 °C THERM.REF variation is 2.52-2.46V respectevely
        // Therm ouput -> For 35-42 °C THERM.OUT variation is 1.75-1.62V respectevely
        // Therm offset -> For 35-42 °C THERM.OFST variation is 0.77-0.88V respectevely

        // Calculate voltage in mV
        float voltage = get_therm_out - get_therm_ofst;

        // Calculate current through the resistor
        float voltage_drop = get_therm_ref - get_therm_out; // in mV
        float current = voltage_drop / k_resistor_value;    // in A

        // Calculate thermistor resistance
        float resistance = voltage / current; // in kΩ

        LOG_INFO("WPT Manager: IPG thermal resistance: %d", resistance);

        // Handle out of range values first
        if (resistance >= TEMP_LOOKUP_TABLE[0].resistance)
        {
            return TEMP_LOOKUP_TABLE[0].temp;
        }
        if (resistance <= TEMP_LOOKUP_TABLE[LOOKUP_TABLE_SIZE - 1].resistance)
        {
            return TEMP_LOOKUP_TABLE[LOOKUP_TABLE_SIZE - 1].temp;
        }

        // Find the appropriate interval in the lookup table
        for (size_t i = 0; i < LOOKUP_TABLE_SIZE - 1; i++)
        {
            if (resistance <= TEMP_LOOKUP_TABLE[i].resistance && resistance >= TEMP_LOOKUP_TABLE[i + 1].resistance)
            {
                // Linear interpolation using floating point for better precision
                float temp_diff = (TEMP_LOOKUP_TABLE[i + 1].temp - TEMP_LOOKUP_TABLE[i].temp);
                float resistance_diff = (TEMP_LOOKUP_TABLE[i].resistance - TEMP_LOOKUP_TABLE[i + 1].resistance);
                float resistance_offset = (TEMP_LOOKUP_TABLE[i].resistance - resistance);

                // Calculate interpolated temperature
                float interpolated_temp = (TEMP_LOOKUP_TABLE[i].temp +
                                           (temp_diff * (resistance_offset / resistance_diff)));

                return interpolated_temp;
            }
        }

        // This should never be reached if the lookup table is properly ordered
        LOG_ERROR("WPT Manager: Temperature interpolation failed - possible lookup table ordering issue");
        return TEMP_LOOKUP_TABLE[LOOKUP_TABLE_SIZE - 1].temp; // Return a safe default
    }

    void WptManager::IpgTemperatureMonitoring(void)
    {
        app::System *pSystem = &app::System::GetInstance();
        WptSubsystem &pWptSubsystem = svc::WptSubsystem::Instance();

        const svc::AdvertisementData_t &advData = svc::BleManager::GetAdvertisementData();
        svc::ChargingStatusParameters_t ChargingStatusParameters = advData.chargingStatusParameters;

        float ipg_temperature = CalculateTemperatureFromBle(ChargingStatusParameters.GET_THERM_REF, ChargingStatusParameters.GET_THERM_OUT, ChargingStatusParameters.GET_THERM_OFST);

        LOG_INFO("WPT Manager: IPG Temperature %d C\n", ipg_temperature);

        // Check if temperature exceeds high threshold
        if (ipg_temperature >= IPG_TEMP_THRESHOLD_HIGH)
        {
            LOG_ERROR("WPT Manager: Critical IPG temperature detected %d.%02d C",
                      (int32_t)ipg_temperature,
                      (int32_t)((ipg_temperature) * 100) % 100);

            // Check if temperature exceeds high threshold
            LOG_ERROR("WPT Manager: Extremely critical temperature detected, Stop power transfer");
            pSystem->mSystemPort.SendEventFromISR(app::SystemPort::Event_e::TURN_OFF, NULL);
            m_is_high_temperature_threshold_exceeded = true;
        }
        // Check if temperature exceeds medium threshold
        // LOG_WARNING for medium threshold breach
        else if (ipg_temperature >= IPG_TEMP_THRESHOLD_MEDIUM && ipg_temperature < IPG_TEMP_THRESHOLD_HIGH)
        {
            LOG_WARNING("WPT Manager: IPG Temperature warning %d.%02d C exceeds medium threshold (%d C)",
                        (int32_t)ipg_temperature,
                        (int32_t)((ipg_temperature) * 100) % 100,
                        IPG_TEMP_THRESHOLD_MEDIUM);

            if (!m_is_high_temperature_threshold_exceeded)
            {
                LOG_ERROR("WPT Manager: Critical temperature detected , Pause power transfer");
                WptPort::SendEventFromISR(WptPort::Event_e::WPT_POWER_OFF, NULL);
                m_is_high_temperature_threshold_exceeded = true;
            }
        }
        // LOG_INFO for low threshold
        else if (ipg_temperature > IPG_TEMP_THRESHOLD_LOW && ipg_temperature < IPG_TEMP_THRESHOLD_MEDIUM)
        {
            LOG_INFO("WPT Manager: IPG Temperature is above the low threshold (%d C) %d.%02d C.",
                     IPG_TEMP_THRESHOLD_LOW,
                     (int32_t)ipg_temperature,
                     (int32_t)((ipg_temperature) * 100) % 100);
        }
        // We're in high temperature state, check if we can return to normal
        // Only switch back when temperature drops below high threshold
        else //(ipg_temperature <= low_threshold)
        {
            LOG_INFO("WPT Manager: IPG Temperature is under the low threshold (%d C) %d.%02d C.",
                     IPG_TEMP_THRESHOLD_LOW,
                     (int32_t)ipg_temperature,
                     (int32_t)((ipg_temperature) * 100) % 100);
            if (m_is_high_temperature_threshold_exceeded)
            {
                LOG_INFO("WPT Manager: IPG Temperature returned to safe level, Resume power transfer");
                WptPort::SendEventFromISR(WptPort::Event_e::WPT_POWER_ON, NULL);
                m_is_high_temperature_threshold_exceeded = false;
            }
        }
    }

    void WptManager::AdjustWptPowerTransfer(uint8_t step)
    {
        LOG_DEBUG("WPT Manager: SetPulseWidthThresholdStep\n");
        WptHalInstance.SetPulseWidthThresholdStep(step);
    }

    void WptManager::ResetPgoodMonitoringStateMachine()
    {
        // Reset all state machine variables to their initial values
        pgood_st_machine_current_state = PgoodState::INIT;
        pgood_st_machine_current_power_level = 0;
        pgood_st_machine_stable_power_level = 0;
        pgood_st_machine_unstability_toggle_count = 0;
        pgood_st_machine_stability_counter = 0;
        pgood_st_machine_fine_tune_attempts = 0;
        pgood_st_machine_last_pgood_status = false;

        LOG_INFO("WPT Manager: PGOOD state machine variables initialized");
    }

    void WptManager::IpgPgoodMonitoring(void)
    {
        // State Machine for PGOOD-based Power Management
        //
        // This function implements a state machine that incrementally adjusts wireless power
        // transmission levels based on the PGOOD signal from the device being charged.
        //
        // The state machine has the following states:
        //
        // 1. INIT: Initial state that resets power to minimum level
        // 2. INCREASING_POWER: Incrementally increases power until PGOOD=1 is detected
        // 3. STABILIZING: Confirms stability at current power level before proceeding
        // 4. FINE_TUNING: Attempts to optimize power level for best charging efficiency
        // 5. STABLE: Maintains stable operation while monitoring for changes

        // Get current PGOOD status from BLE advertisement data
        const svc::AdvertisementData_t &advData = svc::BleManager::GetAdvertisementData();
        svc::ChargingStatusParameters_t ChargingStatusParameters = advData.chargingStatusParameters;
        bool pgood_status = ChargingStatusParameters.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD;

        LOG_INFO("WPT Manager: PGOOD status: %d, State: %d, Power level: %d",
                 pgood_status, static_cast<uint8_t>(pgood_st_machine_current_state), pgood_st_machine_current_power_level);

        // State machine implementation
        switch (pgood_st_machine_current_state)
        {
        // INIT State
        //
        // Purpose: Initialize the power level to minimum and start the power adjustment process
        //
        // Actions:
        // - Set power to minimum level
        // - Log initialization
        // - Transition to INCREASING_POWER state
        //
        // Next States:
        // - Always transitions to INCREASING_POWER
        case PgoodState::INIT:
            // Initialize to minimum power
            pgood_st_machine_current_power_level = MIN_POWER_LEVEL;
            SetPowerLevel(pgood_st_machine_current_power_level);
            LOG_INFO("WPT Manager: Initializing power at minimum level %d", pgood_st_machine_current_power_level);
            pgood_st_machine_current_state = PgoodState::INCREASING_POWER;
            break;

        // INCREASING_POWER State
        //
        // Purpose: Incrementally increase power until PGOOD=1 is detected
        //
        // Actions:
        // - If PGOOD=1: Save current power level as stable and move to STABILIZING
        // - If PGOOD=0 and below max power: Increase power by one step
        // - If PGOOD=0 and at max power: Reset to minimum and try again
        //
        // Next States:
        // - STABILIZING: When PGOOD=1 is detected
        // - INIT: When max power is reached without finding PGOOD=1
        // - Remains in INCREASING_POWER: While increasing power and PGOOD=0
        case PgoodState::INCREASING_POWER:
            if (pgood_status)
            {
                // We found PGOOD=1, now stabilize at this power level
                LOG_INFO("WPT Manager: PGOOD=1 detected at power level %d, stabilizing", pgood_st_machine_current_power_level);
                pgood_st_machine_stable_power_level = pgood_st_machine_current_power_level;
                pgood_st_machine_stability_counter = 0;
                pgood_st_machine_current_state = PgoodState::STABILIZING;
            }
            else if (pgood_st_machine_current_power_level < m_max_power_level)
            {
                // Increase power by one step since PGOOD is still 0
                pgood_st_machine_current_power_level++;
                SetPowerLevel(pgood_st_machine_current_power_level);
                LOG_INFO("WPT Manager: Increasing power to level %d", pgood_st_machine_current_power_level);
            }
            else
            {
                // We've reached max power but still no PGOOD=1
                LOG_WARNING("WPT Manager: Reached maximum power level without PGOOD=1");
                // Reset to minimum and try again
                pgood_st_machine_current_power_level = MIN_POWER_LEVEL;
                SetPowerLevel(pgood_st_machine_current_power_level);
                LOG_INFO("WPT Manager: Resetting to minimum power level %d", pgood_st_machine_current_power_level);
            }
            break;

        // STABILIZING State
        //
        // Purpose: Confirm stability at current power level before attempting optimization
        //
        // Actions:
        // - If PGOOD=1: Increment stability counter
        // - If stability threshold reached: Move to FINE_TUNING or STABLE based on power level
        // - If PGOOD=0: Power level is unstable, revert to previous stable level or INIT
        //
        // Next States:
        // - FINE_TUNING: When stability threshold is reached and below max power
        // - STABLE: When stability threshold is reached at max power
        // - INIT: When no stable level is found
        // - Remains in STABILIZING: While counting stable cycles
        case PgoodState::STABILIZING:
            if (pgood_status)
            {
                // PGOOD remains at 1, counting stable cycles
                pgood_st_machine_stability_counter++;
                LOG_INFO("WPT Manager: PGOOD stable for %d cycles at power level %d",
                         pgood_st_machine_stability_counter, pgood_st_machine_current_power_level);

                if (pgood_st_machine_stability_counter >= STABILITY_THRESHOLD)
                {
                    // We have stable PGOOD for sufficient cycles
                    if (pgood_st_machine_current_power_level < m_max_power_level)
                    {
                        // Try fine-tuning with a bit more power
                        LOG_INFO("WPT Manager: Stable PGOOD achieved, attempting fine-tuning");
                        pgood_st_machine_fine_tune_attempts = 0;
                        pgood_st_machine_unstability_toggle_count = 0;
                        pgood_st_machine_current_state = PgoodState::FINE_TUNING;
                    }
                    else
                    {
                        // Already at max power with stable PGOOD
                        LOG_INFO("WPT Manager: Stable PGOOD at maximum power level");
                        pgood_st_machine_current_state = PgoodState::STABLE;
                    }
                }
            }
            else
            {
                // PGOOD toggled back to 0, current level is unstable
                // No stable level found, restart process
                LOG_WARNING("WPT Manager: No stable level found, restarting");
                pgood_st_machine_current_state = PgoodState::INIT;
            }
            break;

        // FINE_TUNING State
        //
        // Purpose: Optimize power level by attempting incremental increases
        //
        // Actions:
        // - Monitor PGOOD toggle behavior to detect stability boundaries
        // - If PGOOD toggles frequently: Try another power level or revert to stable level
        // - If PGOOD consistently 1: Count stability cycles for new potential stable level
        // - If PGOOD consistently 0: Reduce power level and reset counters
        //
        // Next States:
        // - STABLE: When new optimal level is found or fine-tuning fails
        // - Remains in FINE_TUNING: While testing different power levels
        case PgoodState::FINE_TUNING:
            // Check if PGOOD status changed from previous reading
            if (pgood_status != pgood_st_machine_last_pgood_status)
            {
                pgood_st_machine_unstability_toggle_count++;
                LOG_INFO("WPT Manager: PGOOD toggled during fine-tuning, count: %d", pgood_st_machine_unstability_toggle_count);

                // If PGOOD is toggling too much, fine-tuning this power level isn't working
                if (pgood_st_machine_unstability_toggle_count >= MAX_COUNT_TOGGLING)
                {
                    if (pgood_st_machine_fine_tune_attempts < MAX_FINE_TUNE_STEPS)
                    {
                        // Try another fine-tuning step
                        pgood_st_machine_fine_tune_attempts++;
                        pgood_st_machine_current_power_level++;

                        if (pgood_st_machine_current_power_level > m_max_power_level)
                        {
                            pgood_st_machine_current_power_level = m_max_power_level;
                        }

                        SetPowerLevel(pgood_st_machine_current_power_level);
                        LOG_INFO("WPT Manager: Trying another fine-tune step, power level %d", pgood_st_machine_current_power_level);
                        pgood_st_machine_unstability_toggle_count = 0;
                    }
                    else
                    {
                        // Fine-tuning failed, revert to stable power level
                        LOG_WARNING("WPT Manager: Fine-tuning failed after %d attempts", pgood_st_machine_fine_tune_attempts);
                        pgood_st_machine_current_power_level = pgood_st_machine_stable_power_level;
                        SetPowerLevel(pgood_st_machine_current_power_level);
                        LOG_INFO("WPT Manager: Reverting to stable power level %d", pgood_st_machine_current_power_level);
                        pgood_st_machine_current_state = PgoodState::STABLE;
                    }
                }
            }
            else if (pgood_status)
            {
                // PGOOD is consistently 1, this might be a better stable point
                pgood_st_machine_stability_counter++;

                if (pgood_st_machine_stability_counter >= STABILITY_THRESHOLD)
                {
                    LOG_INFO("WPT Manager: Found improved stable level at %d", pgood_st_machine_current_power_level);
                    pgood_st_machine_stable_power_level = pgood_st_machine_current_power_level;
                    pgood_st_machine_current_state = PgoodState::STABLE;
                }
            }
            else
            {
                // PGOOD is consistently 0, this level is too high
                LOG_WARNING("WPT Manager: Power level %d is too high, PGOOD consistently 0", pgood_st_machine_current_power_level);
                if (pgood_st_machine_current_power_level > MIN_POWER_LEVEL)
                {
                    pgood_st_machine_current_power_level--;
                }
                SetPowerLevel(pgood_st_machine_current_power_level);
                LOG_INFO("WPT Manager: Reducing to power level %d", pgood_st_machine_current_power_level);
                pgood_st_machine_unstability_toggle_count = 0;
                pgood_st_machine_stability_counter = 0;
            }
            break;

        // STABLE State
        //
        // Purpose: Maintain stable operation while monitoring for changes in PGOOD status
        //
        // Actions:
        // - Monitor PGOOD status for changes
        // - If PGOOD drops to 0: Stability is lost, return to power adjustment process
        // - If PGOOD remains 1: Log stable operation periodically
        //
        // Next States:
        // - INCREASING_POWER: When stability is lost (PGOOD=0)
        // - Remains in STABLE: While PGOOD=1
        case PgoodState::STABLE:
            // Monitor for changes in stable operation
            if (!pgood_status)
            {
                // PGOOD dropped to 0, stability lost
                LOG_WARNING("WPT Manager: Stability lost at power level %d", pgood_st_machine_current_power_level);
                // Try to recover by going back to increasing power
                pgood_st_machine_current_state = PgoodState::INCREASING_POWER;
            }
            else
            {
                // Periodically log that we're in stable operation
                LOG_INFO("WPT Manager: Stable operation at power level %d", pgood_st_machine_current_power_level);
            }
            break;
        }

        // Save current PGOOD status for next iteration
        pgood_st_machine_last_pgood_status = pgood_status;
    }

    void WptManager::SetPowerLevel(uint8_t level)
    {
        // Send event to adjust power level
        WptPort::SendEventFromISR(WptPort::Event_e::WPT_ADJUST_POWER, static_cast<uint32_t>(level));
    }

    void WptManager::StatusTempPgoodMonitoring(TimerHandle_t xTimer)
    {
        LOG_DEBUG("WPT Manager: StatusTempPgoodMonitoring\n");

        IpgTemperatureMonitoring();

        IpgPgoodMonitoring();

        LOG_FLUSH();
    }
}