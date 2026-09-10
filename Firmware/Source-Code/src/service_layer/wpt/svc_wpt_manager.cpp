/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_manager.cpp
 * @brief WptManager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_wpt_manager.h"

#include "eda_manager_log_config.h"
#include "hal_dac.h"
#include "svc_ble_subsystem.h"
#include "svc_debug_console.h"

namespace svc
{
    uint8_t static m_max_power_level;

    uint8_t WptManager::m_pause_reasons = 0;
    bool WptManager::m_coil_enabled = false;
    uint16_t WptManager::m_thermal_pause_ticks = 0;
    uint16_t WptManager::m_ovp_pause_ticks = 0;

    uint8_t WptManager::m_level = WptManager::COLD_START_LEVEL;
    bool WptManager::m_floor_found = false;
    uint8_t WptManager::m_ovp_ceiling = WptManager::LEVEL_INVALID;
    uint8_t WptManager::m_pgood_floor = WptManager::LEVEL_INVALID;
    uint8_t WptManager::m_blank_cycles = 0;
    uint8_t WptManager::m_pgood_low_count = 0;
    uint32_t WptManager::m_last_adv_count = 0;
    bool WptManager::m_loop_initialized = false;
    uint32_t WptManager::m_ovp_last_adv_count = 0;
    bool WptManager::m_chg1_ovp_logged = false;
    bool WptManager::m_chg2_ovp_logged = false;
    uint8_t WptManager::m_level_max_this_tick = WptManager::COLD_START_LEVEL;
    uint8_t WptManager::m_level_max_last_tick = WptManager::COLD_START_LEVEL;

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

    eda::Timer WptManager::mFaultTimer("WptFaultMonitor", FAULT_MONITOR_PERIOD_MS, 1, FaultMonitoring);

    eda::Timer WptManager::mPowerCtrlTimer("WptPowerCtrl", POWER_CTRL_PERIOD_MS, 1, PowerControlMonitoring);

    eda::Timer WptManager::mColdStartEscalateTimer("WptColdStart", COLD_START_ESCALATE_MS, 0, ColdStartEscalate);

    void WptManager::Init()
    {
        LOG_DEBUG("WPT Manager: Init\n");
        ConfigureGpios();
        DacHalInstance.Init();
        WptHalInstance.Init();
        m_max_power_level = WptHalInstance.GetMaxPulseWidthThresholdStep();
        ResetPowerControl();
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
         static constexpr hal::Gpio::gpio_config_t gpioCtdConfig = {
            .pin_number = PIN_WPT_CTD,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_OUTPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_PULLDOWN,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(gpioCtdConfig);
        hal::Gpio::Write(PIN_WPT_CTD, 1); 

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
        // A fault pause outranks a state entry. StateSlowCharge -> StateCharging on
        // WPT_POWER_ON re-runs Entry(), and without this guard that would turn the
        // coil back on underneath an active thermal or OVP pause.
        if (m_pause_reasons != 0)
        {
            LOG_WARNING("WPT Manager: EnableWpt suppressed, fault mask 0x%02X still set\n", m_pause_reasons);
        }
        else
        {
            WptHalInstance.Enable();
            m_coil_enabled = true;
        }

        StartStatusTimeoutTimer();
        StartStatusMonitoring();
        LOG_DEBUG("WPT Manager: EnableWpt\n");
    }

    void WptManager::DisableWpt()
    {
        WptHalInstance.Disable();
        m_coil_enabled = false;
        StopStatusMonitoring();
        StopIpgTemperaturePgoodMonitoringTimer();
        mColdStartEscalateTimer.Stop();
        ResetPowerControl();
        LOG_DEBUG("WPT Manager: DisableWpt\n");
    }

    void WptManager::PauseWpt(uint8_t reason)
    {
        // Only the first reason actually stops the coil; a second concurrent fault
        // must not re-issue Disable(), and more importantly must not be able to
        // resume on its own later while the first one is still asserted.
        const bool was_running = (m_pause_reasons == 0);

        m_pause_reasons |= reason;

        if (was_running)
        {
            WptHalInstance.Disable();
            m_coil_enabled = false;
        }

        // Deliberately does not touch mFaultTimer or mPowerCtrlTimer: monitoring has
        // to keep running through a pause or the recovery condition can never be
        // observed. This is the decoupling that made thermal auto-resume work.
        LOG_WARNING("WPT Manager: PauseWpt reason 0x%02X, mask now 0x%02X, coil %s\n",
                    reason, m_pause_reasons, was_running ? "disabled" : "already off");
    }

    void WptManager::ResumeWpt(uint8_t reason)
    {
        m_pause_reasons &= static_cast<uint8_t>(~reason);

        if (m_pause_reasons == 0)
        {
            WptHalInstance.Enable();
            m_coil_enabled = true;
            LOG_INFO("WPT Manager: ResumeWpt reason 0x%02X cleared, coil re-enabled at level %d\n",
                     reason, m_level);
        }
        else
        {
            LOG_WARNING("WPT Manager: ResumeWpt reason 0x%02X cleared but mask still 0x%02X, coil stays off\n",
                        reason, m_pause_reasons);
        }
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
        // Fault sampling and power control run at different rates but share a
        // lifetime, so callers still start and stop them as one unit.
        LOG_DEBUG("WPT Manager: StartIpgTemperatureMonitoringTimer\n");
        mFaultTimer.Start();
        mPowerCtrlTimer.Start();
    }

    void WptManager::StartFaultMonitoringOnly()
    {
        // Fault sampling without the power search. The manual bench state needs the
        // thermal and OVP thresholds evaluated and logged, but nothing may move the
        // level out from under the operator.
        LOG_INFO("WPT Manager: Fault monitoring only (power control timer not started)\n");
        mFaultTimer.Start();
    }

#if WPT_MANUAL_DEBUG_MODE
    void WptManager::EnterManualIdle()
    {
        // DisableWpt() posts the cold-start escalation timer's stop before it calls
        // ResetPowerControl(). The timer task outranks this one, so an escalation
        // that had already expired runs before the reset rather than after it,
        // and the level still ends at COLD_START_LEVEL.
        DisableWpt();
        StartFaultMonitoringOnly();
        LOG_WARNING("WPT Manager: Manual idle - coil off, level %d, fault monitoring only\n", m_level);
    }
#endif

    void WptManager::StopIpgTemperaturePgoodMonitoringTimer()
    {
        LOG_DEBUG("WPT Manager: StopIpgTemperatureMonitoringTimer\n");
        mFaultTimer.Stop();
        mPowerCtrlTimer.Stop();
    }

    void WptManager::StartColdStartEscalation()
    {
        LOG_INFO("WPT Manager: Cold start at level %d, escalating to max in %d ms if no IPG advertisement\n",
                 COLD_START_LEVEL, COLD_START_ESCALATE_MS);
        SetPowerLevel(COLD_START_LEVEL);
        mColdStartEscalateTimer.Start();
    }

    void WptManager::ColdStartEscalate(TimerHandle_t xTimer)
    {
        // Self-guarding: if an advertisement arrived while this timer was pending,
        // the closed loop now owns the power level and must not be overridden.
        // That removes any need to cancel this timer on the BLE-found path.
        if (svc::BleManager::GetAdvertisementCount() != 0)
        {
            LOG_INFO("WPT Manager: Cold start escalation skipped, IPG already advertising\n");
            return;
        }

        LOG_WARNING("WPT Manager: Cold start escalating to maximum power, still no IPG advertisement\n");
        SetPowerLevel(LEVEL_REQUEST_MAX);
    }

    bool WptManager::IsThermReadingPlausible(uint16_t get_therm_ref,
                                             uint16_t get_therm_out,
                                             uint16_t get_therm_ofst)
    {
        // The IPG packs each value as (mV / 10) into one byte. REF is clamped at
        // 255 (2550 mV) but OUT and OFST are not, so an OUT of 2560 mV or more
        // wraps - what BLE viewers show as "n/a". The IPG's ADC cannot read above
        // 3.6 V, so a wrapped OUT always decodes to 0..THERM_OUT_WRAP_MAX_MV, and
        // the floor below rejects every wrap whatever OFST happens to be.

        // Zero: a wrapped byte, or the IPG's thermistor circuit unpowered.
        if (get_therm_ref == 0 || get_therm_out == 0 || get_therm_ofst == 0)
        {
            return false;
        }

        // Wrapped OUT. A genuine OUT this low would need the thermistor below
        // ~9 kOhm (~90 C at the design OFST of ~0.8 V) - unreachable without
        // first passing through the 41 C trip on an earlier reading.
        if (get_therm_out <= THERM_OUT_WRAP_MAX_MV)
        {
            return false;
        }

        // No voltage across the thermistor.
        if (get_therm_out <= get_therm_ofst)
        {
            return false;
        }

        // No current through the 49.9 kOhm sense resistor. Also catches OUT at
        // 2550-2559 mV, which encodes as 255 - the same byte as the clamped REF.
        if (get_therm_out >= get_therm_ref)
        {
            return false;
        }

        return true;
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

        LOG_INFO("WPT Manager: IPG thermistor resistance: %d", (int32_t)resistance);

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
        // With no advertisement parsed yet the thermistor fields are all zero, which
        // makes the resistance calculation degenerate and falls through to the
        // table's top entry (50 C) - an instant spurious thermal pause.
        if (svc::BleManager::GetAdvertisementCount() == 0)
        {
            return;
        }

        const svc::AdvertisementData_t &advData = svc::BleManager::GetAdvertisementData();
        svc::ChargingStatusParameters_t ChargingStatusParameters = advData.chargingStatusParameters;

        const bool thermally_paused = (m_pause_reasons & PAUSE_THERMAL) != 0;

        // The dwell measures time, not readings, so it keeps counting through
        // implausible samples below.
        if (thermally_paused)
        {
            m_thermal_pause_ticks++;
        }

        // An implausible reading carries no temperature at all, so it can neither
        // pause nor resume: whatever state we are in is held until a real reading
        // arrives. Without this, a wrapped OUT byte computes as a negative
        // resistance, maps to the table's hot end (50 C) and pauses on its own.
        if (!IsThermReadingPlausible(ChargingStatusParameters.GET_THERM_REF,
                                     ChargingStatusParameters.GET_THERM_OUT,
                                     ChargingStatusParameters.GET_THERM_OFST))
        {
            LOG_WARNING("WPT Manager: IPG thermistor reading implausible (ref=%d out=%d ofst=%d mV), ignored - no thermal decision this tick\n",
                        ChargingStatusParameters.GET_THERM_REF,
                        ChargingStatusParameters.GET_THERM_OUT,
                        ChargingStatusParameters.GET_THERM_OFST);
            return;
        }

        float ipg_temperature = CalculateTemperatureFromBle(ChargingStatusParameters.GET_THERM_REF, ChargingStatusParameters.GET_THERM_OUT, ChargingStatusParameters.GET_THERM_OFST);

        LOG_INFO("WPT Manager: IPG Temperature %d.%02d C\n",
           (int32_t)ipg_temperature, (int32_t)(ipg_temperature * 100) % 100);

        LOG_INFO("WPT Manager: therm raw ref=%d out=%d ofst=%d mV\n",
           ChargingStatusParameters.GET_THERM_REF,
           ChargingStatusParameters.GET_THERM_OUT,
           ChargingStatusParameters.GET_THERM_OFST);

        if (!thermally_paused)
        {
            if (ipg_temperature >= IPG_TEMP_THRESHOLD_PAUSE)
            {
#if WPT_MANUAL_DEBUG_MODE
                // Warn only: the threshold is still evaluated and reported, but the
                // coil is left exactly where the operator put it. The IPG's own
                // 42 C gate is unaffected and still applies.
                if (DebugConsole::IsManual())
                {
                    LOG_WARNING("[MANUAL] WARN thermal: IPG %d.%02d C >= %d C pause threshold (ignored)\n",
                                (int32_t)ipg_temperature,
                                (int32_t)((ipg_temperature) * 100) % 100,
                                IPG_TEMP_THRESHOLD_PAUSE);
                    return;
                }
#endif

                LOG_ERROR("WPT Manager: IPG temperature %d.%02d C reached pause threshold (%d C), pausing power transfer",
                          (int32_t)ipg_temperature,
                          (int32_t)((ipg_temperature) * 100) % 100,
                          IPG_TEMP_THRESHOLD_PAUSE);
                m_thermal_pause_ticks = 0;
                WptPort::SendEventFromISR(WptPort::Event_e::WPT_FAULT_PAUSE,
                                          static_cast<uint32_t>(PAUSE_THERMAL));
            }
            return;
        }

        // Held in a thermal pause. Resume needs both a minimum dwell and a genuine
        // recovery below the lower hysteresis threshold - the dwell alone would let
        // us re-enable into an implant that is still at the limit, producing a
        // power burst cycle instead of a controlled hold. (The dwell counter was
        // advanced at the top, before the plausibility check.)
        if (m_thermal_pause_ticks < THERMAL_PAUSE_MIN_TICKS)
        {
            return;
        }

        if (ipg_temperature > IPG_TEMP_THRESHOLD_RESUME)
        {
            return;
        }

        LOG_INFO("WPT Manager: IPG temperature %d.%02d C at/below resume threshold (%d C) after %d ticks, resuming power transfer",
                 (int32_t)ipg_temperature,
                 (int32_t)((ipg_temperature) * 100) % 100,
                 IPG_TEMP_THRESHOLD_RESUME,
                 m_thermal_pause_ticks);

        // A thermal trip is direct evidence that the level we settled on delivers
        // more power than this coupling needs, so re-arm the downward search. This
        // is the only event that reopens it once a floor has been found.
        m_floor_found = false;
        m_blank_cycles = BLANK_CYCLES_AFTER_FAULT;

        WptPort::SendEventFromISR(WptPort::Event_e::WPT_FAULT_RESUME,
                                  static_cast<uint32_t>(PAUSE_THERMAL));
    }

    void WptManager::IpgOvpMonitoring(void)
    {
        // Rotate the fault-attribution window first, on every tick and before any
        // early return, so it always spans exactly the current and previous tick.
        const uint8_t fault_level = (m_level_max_this_tick > m_level_max_last_tick)
                                        ? m_level_max_this_tick
                                        : m_level_max_last_tick;
        m_level_max_last_tick = m_level_max_this_tick;
        m_level_max_this_tick = m_level;

        const uint32_t adv_count = svc::BleManager::GetAdvertisementCount();

        // mAdvertisementData is zero-initialised and the IPG's fault bits are
        // active-low, so before the first advertisement every fault reads as
        // asserted. Without this guard the charger pauses for OVP at every startup.
        if (adv_count == 0)
        {
            return;
        }

        // Only a new advertisement can report a new fault. Re-reading the same one
        // on the next tick would charge an old fault to whatever level is in force
        // by then - and after scanning stops, to every level for the rest of the
        // session.
        const bool fresh = (adv_count != m_ovp_last_adv_count);
        m_ovp_last_adv_count = adv_count;

        const svc::AdvertisementData_t &advData = svc::BleManager::GetAdvertisementData();
        const svc::ChargingStatusParameters_t &p = advData.chargingStatusParameters;

        // Raw active-low bits straight from the IPG MSD GPIO byte: 1 = OK,
        // 0 = asserted. The IPG packs the pin level unmodified
        // (setBitFromGpioState in app_mode_ble_active.c), so these compares are
        // the only inversion anywhere between the IPG pin and this decision.
        //
        // Only VRECT_OVPn acts. It is a FET threshold on VRECT/17 - the rectifier
        // voltage the coil produces - and so the only fault that coil power causes
        // and that reducing coil power can clear.
        //
        // CHGx_OVP_ERRn are logged and nothing more. Each is a battery-node
        // comparator (VBATxCHG >= 4.277 V) that disconnects its battery in hardware
        // by itself, and the IPG additionally pauses its own converter on it. The
        // LTC4065 holds its float voltage regardless of input power, so no PTH
        // level can clear a genuine battery OVP; and the comparator runs from
        // VCHG_RAIL, so its output is not guaranteed while that rail is collapsed
        // (PGOOD = 0) - pausing on it caused spurious pauses at low power. If the
        // IPG's converter shutdown leaves the coil with nowhere to put its energy,
        // VRECT rises and VRECT_OVPn brings the charger in through the path above.
        const bool vrect_ovp = (p.GET_VRECT_OVP == 0);
        const bool chg1_ovp = (p.GET_CHG1_OVP_ERR == 0);
        const bool chg2_ovp = (p.GET_CHG2_OVP_ERR == 0);

        if (fresh && ((chg1_ovp != m_chg1_ovp_logged) || (chg2_ovp != m_chg2_ovp_logged)))
        {
            // Logged on change only - these never alter the coil, so a line per
            // advertisement would be noise. PGOOD is included because a flag that
            // asserts only while PGOOD = 0 points at the VCHG_RAIL collapse rather
            // than a real battery over-voltage.
            if (chg1_ovp || chg2_ovp)
            {
                LOG_WARNING("WPT Manager: IPG battery OVP flag asserted (CHG1_OVP_ERRn=%d CHG2_OVP_ERRn=%d, 0 = fault) at level %d, PGOOD=%d - logged only, coil unaffected\n",
                            p.GET_CHG1_OVP_ERR, p.GET_CHG2_OVP_ERR, m_level,
                            p.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD);
            }
            else
            {
                LOG_INFO("WPT Manager: IPG battery OVP flags cleared (CHG1_OVP_ERRn=%d CHG2_OVP_ERRn=%d) at level %d, PGOOD=%d\n",
                         p.GET_CHG1_OVP_ERR, p.GET_CHG2_OVP_ERR, m_level,
                         p.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD);
            }

            m_chg1_ovp_logged = chg1_ovp;
            m_chg2_ovp_logged = chg2_ovp;
        }

        const bool ovp_paused = (m_pause_reasons & PAUSE_OVP) != 0;

        if (!ovp_paused)
        {
            if (!fresh || !vrect_ovp)
            {
                return;
            }

            RecordOvpCeiling(fault_level);

#if WPT_MANUAL_DEBUG_MODE
            // Warn only. The ceiling above is still recorded, so a later return to
            // automatic mode still knows which level faulted, but nothing pauses
            // the coil and nothing steps the level down.
            if (DebugConsole::IsManual())
            {
                LOG_WARNING("[MANUAL] WARN ovp: VRECT_OVPn=0 at level %d, charged to %d, ceiling %d, ignored\n",
                            m_level, fault_level, m_ovp_ceiling);
                return;
            }
#endif

            LOG_ERROR("WPT Manager: IPG rectifier OVP (VRECT_OVPn=0) at level %d, charged to %d, ceiling %d, pausing",
                      m_level, fault_level, m_ovp_ceiling);

            m_ovp_pause_ticks = 0;
            WptPort::SendEventFromISR(WptPort::Event_e::WPT_FAULT_PAUSE,
                                      static_cast<uint32_t>(PAUSE_OVP));
            return;
        }

        m_ovp_pause_ticks++;

        if (m_ovp_pause_ticks < OVP_PAUSE_MIN_TICKS)
        {
            return;
        }

        if (vrect_ovp)
        {
            LOG_WARNING("WPT Manager: VRECT_OVPn still 0 after %d ticks, staying paused\n", m_ovp_pause_ticks);
            return;
        }

        // Back off one step before restoring power. The IPG's own comment on its
        // 5 s pause hold says it expects the charger to have reduced coil power by
        // the time it re-enables; without that this pair just re-trips.
        if (m_level > MIN_POWER_LEVEL)
        {
            SetPowerLevel(m_level - 1);
        }

        // The IPG's VCHG rail needs a moment to come back after it re-enables, so
        // PGOOD reads 0 with OVP already cleared. That is exactly the "add power"
        // condition, which would undo the back-off - so skip a control cycle.
        m_blank_cycles = BLANK_CYCLES_AFTER_FAULT;

        LOG_INFO("WPT Manager: VRECT_OVPn cleared after %d ticks, resuming one step down at level %d\n",
                 m_ovp_pause_ticks, m_level);

        WptPort::SendEventFromISR(WptPort::Event_e::WPT_FAULT_RESUME,
                                  static_cast<uint32_t>(PAUSE_OVP));
    }

    void WptManager::AdjustWptPowerTransfer(uint8_t step)
    {
        LOG_DEBUG("WPT Manager: SetPulseWidthThresholdStep\n");
        WptHalInstance.SetPulseWidthThresholdStep(step);
    }

    uint8_t WptManager::GetMaxPowerLevel()
    {
        return m_max_power_level;
    }

    void WptManager::SetPowerLevelManual(uint8_t level)
    {
        SetPowerLevel(level);
    }

    void WptManager::RearmPowerSearch()
    {
        m_floor_found = false;
        m_pgood_low_count = 0;

        // Skip a cycle so the first automatic decision is made on telemetry that
        // reflects the level actually in force, not one sampled mid-handover.
        m_blank_cycles = BLANK_CYCLES_AFTER_FAULT;

        LOG_INFO("WPT Manager: Power search re-armed at level %d\n", m_level);
    }

    void WptManager::ResetPowerControl()
    {
        m_pause_reasons = 0;
        m_thermal_pause_ticks = 0;
        m_ovp_pause_ticks = 0;

        m_level = COLD_START_LEVEL;
        m_floor_found = false;
        m_ovp_ceiling = LEVEL_INVALID;
        m_pgood_floor = LEVEL_INVALID;
        m_blank_cycles = 0;
        m_pgood_low_count = 0;
        m_last_adv_count = 0;
        m_loop_initialized = false;
        // Forget the logged battery OVP state so a flag still asserted when the
        // next session starts is reported again there.
        m_chg1_ovp_logged = false;
        m_chg2_ovp_logged = false;
        m_level_max_this_tick = COLD_START_LEVEL;
        m_level_max_last_tick = COLD_START_LEVEL;

        LOG_INFO("WPT Manager: Power control reset, level %d\n", m_level);
    }

    void WptManager::RecordOvpCeiling(uint8_t level)
    {
        // Remember the lowest level that has ever faulted. Without this the
        // "PGOOD is low, add power" rule walks straight back into the level we
        // just tripped on, and the two controllers oscillate indefinitely.
        if (m_ovp_ceiling == LEVEL_INVALID || level < m_ovp_ceiling)
        {
            m_ovp_ceiling = level;
        }
    }

    bool WptManager::IsPowerWindowEmpty()
    {
        // At very close coupling the level that trips OVP can sit at or below the
        // level PGOOD needs, so no setting satisfies the IPG without faulting it.
        // Detecting that is what stops the loop ping-ponging between two adjacent
        // steps for the whole session.
        return (m_ovp_ceiling != LEVEL_INVALID) &&
               (m_pgood_floor != LEVEL_INVALID) &&
               ((m_pgood_floor + 1) >= m_ovp_ceiling);
    }

    void WptManager::PowerControlMonitoring(TimerHandle_t xTimer)
    {
        // Bidirectional search for the lowest PTH ceiling the IPG still accepts.
        //
        // PGOOD alone is ambiguous: the IPG drops VCHG_DISABLE and therefore PGOOD
        // both when it is receiving too little power AND when its own OVP
        // protection has fired because it is receiving too much. Those need
        // opposite corrections. This function only ever sees the first case,
        // because faults are owned entirely by the fault timer below and it bails
        // out whenever one is active - so here PGOOD=0 unambiguously means "more".

        if (m_pause_reasons != 0)
        {
            LOG_DEBUG("WPT Manager: Power control skipped, fault mask 0x%02X owns the level\n", m_pause_reasons);
            return;
        }

        if (m_blank_cycles > 0)
        {
            m_blank_cycles--;
            LOG_DEBUG("WPT Manager: Power control blanked after fault resume, %d cycles left\n", m_blank_cycles);
            return;
        }

        const uint32_t adv_count = svc::BleManager::GetAdvertisementCount();

        // No fresh telemetry since the last decision, so the last change has not
        // been observed yet. Also covers cold start, where the count stays 0 and
        // the open-loop escalation owns the level instead.
        if (adv_count == m_last_adv_count)
        {
            LOG_WARNING("WPT Manager: Power control skipped, no new IPG advertisement since last step\n");
            return;
        }

        m_last_adv_count = adv_count;

        // First cycle with real telemetry: take the level back to the mid-range
        // start, wherever the open-loop cold-start attempt happened to leave it.
        if (!m_loop_initialized)
        {
            m_loop_initialized = true;
            SetPowerLevel(COLD_START_LEVEL);
            LOG_INFO("WPT Manager: Closed loop starting at level %d\n", m_level);
            return;
        }

        const svc::AdvertisementData_t &advData = svc::BleManager::GetAdvertisementData();
        const bool pgood = (advData.chargingStatusParameters.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD == 1);

        LOG_INFO("WPT Manager: Power control level=%d pgood=%d floor_found=%d ovp_ceiling=%d pgood_floor=%d",
                 m_level, pgood, m_floor_found, m_ovp_ceiling, m_pgood_floor);

        if (pgood)
        {
            m_pgood_low_count = 0;

            if (m_floor_found)
            {
                LOG_INFO("WPT Manager: Holding at level %d\n", m_level);
                return;
            }

            if (m_level > MIN_POWER_LEVEL)
            {
                SetPowerLevel(m_level - 1);
                LOG_INFO("WPT Manager: PGOOD satisfied, stepping down to level %d\n", m_level);
            }
            else
            {
                m_floor_found = true;
                LOG_INFO("WPT Manager: PGOOD satisfied at minimum level, floor found\n");
            }

            return;
        }

        // Reject single-sample dropouts. A real insufficiency is only delayed by
        // one cycle, but transient noise no longer moves the level at all.
        m_pgood_low_count++;

        if (m_pgood_low_count < PGOOD_LOW_CONFIRM)
        {
            LOG_WARNING("WPT Manager: PGOOD low %d/%d at level %d, waiting for confirmation\n",
                        m_pgood_low_count, PGOOD_LOW_CONFIRM, m_level);
            return;
        }

        m_pgood_low_count = 0;
        m_pgood_floor = m_level;

        // Confirmed insufficiency ends the downward search: this level is below
        // what the IPG needs, so there is nothing lower worth trying.
        m_floor_found = true;

        if (IsPowerWindowEmpty())
        {
            // Settle at the highest level that has not faulted and stop searching.
            // Undervoltage charging is slow but stable; oscillating between a
            // faulting level and an insufficient one delivers nothing at all.
            const uint8_t best = (m_ovp_ceiling > MIN_POWER_LEVEL)
                                     ? static_cast<uint8_t>(m_ovp_ceiling - 1)
                                     : MIN_POWER_LEVEL;

            LOG_ERROR("WPT Manager: No viable power level - OVP ceiling %d at or below PGOOD floor %d. Clamping to %d",
                      m_ovp_ceiling, m_pgood_floor, best);

            if (m_level != best)
            {
                SetPowerLevel(best);
            }

            return;
        }

        if (m_level >= m_max_power_level)
        {
            LOG_WARNING("WPT Manager: PGOOD low at maximum level %d, nothing further to give\n", m_level);
            return;
        }

        // Climbing past a level that has already faulted the IPG is prevented by
        // IsPowerWindowEmpty() above: m_pgood_floor was just set to m_level, so
        // "the next step reaches the OVP ceiling" and "the window is empty" are the
        // same condition, and the empty-window branch has already returned.
        SetPowerLevel(m_level + 1);
        LOG_INFO("WPT Manager: PGOOD low, stepping up to level %d\n", m_level);
    }

    void WptManager::SetPowerLevel(uint8_t level)
    {
        // Normalise before recording so m_level always names a real step. The HAL
        // clamps anything above the maximum, so LEVEL_REQUEST_MAX must not be
        // stored verbatim or every later comparison against it would be wrong.
        if (level > m_max_power_level)
        {
            level = m_max_power_level;
        }

        m_level = level;

        // Feed the OVP attribution window (see m_level_max_this_tick). Only ever
        // raised here; IpgOvpMonitoring() rotates it on every fault tick.
        if (level > m_level_max_this_tick)
        {
            m_level_max_this_tick = level;
        }

        WptPort::SendEventFromISR(WptPort::Event_e::WPT_ADJUST_POWER, static_cast<uint32_t>(level));
    }

    void WptManager::FaultMonitoring(TimerHandle_t xTimer)
    {
        LOG_DEBUG("WPT Manager: FaultMonitoring\n");

        // OVP first: it is the condition the IPG gives us the least time to react
        // to, and it is what disambiguates the PGOOD reading the power control
        // timer will make on its next tick.
        IpgOvpMonitoring();

        IpgTemperatureMonitoring();

#if WPT_MANUAL_DEBUG_MODE
        // Attribution: with every charger-side cutoff disabled, this is the only
        // way to tell an IPG that has shut itself down from a command that never
        // took effect.
        if (DebugConsole::IsManual())
        {
            DebugConsole::LogIpgBits();
        }
#endif

        LOG_FLUSH();
    }
}