/**
 * @name Hornet / WPT Charger
 * @file svc_debug_console.cpp
 * @brief Manual power-control console over the SEGGER RTT down-channel
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_debug_console.h"

#if WPT_MANUAL_DEBUG_MODE

#include "app_port.h"
#include "app_system.h"
#include "eda_manager_log_config.h"
#include "hal_wpt.h"
#include "svc_ble_manager.h"
#include "svc_pmc_port.h"
#include "svc_wpt_manager.h"
#include "svc_wpt_port.h"

#include "SEGGER_RTT.h"

namespace svc
{
    bool DebugConsole::m_manual = false;
    uint32_t DebugConsole::m_idle_ms = 0;
    bool DebugConsole::m_deadman_tripped = false;

    eda::Timer DebugConsole::mPollTimer("DbgConsole", WPT_MANUAL_POLL_PERIOD_MS, 1, Poll);

    void DebugConsole::Init()
    {
        mPollTimer.Start();
        LOG_WARNING("=====================================================\n");
        LOG_WARNING("[MANUAL] DEBUG BUILD - manual control available\n");
        LOG_WARNING("[MANUAL] Running normal automatic control. Press 'm' to take over.\n");
        LOG_WARNING("=====================================================\n");
    }

    void DebugConsole::Poll(TimerHandle_t xTimer)
    {
        // Drain everything buffered rather than one key per tick, so a burst of
        // keystrokes cannot lag behind the operator.
        while (SEGGER_RTT_HasKey())
        {
            const int key = SEGGER_RTT_GetKey();

            if (key < 0)
            {
                break;
            }

            m_idle_ms = 0;

            // Any key ends a dead-man pause, but the operator still has to press
            // 's' to put power back on - silently restoring the coil on a stray
            // keystroke is exactly the surprise this mode exists to avoid.
            if (m_deadman_tripped)
            {
                m_deadman_tripped = false;
                LOG_WARNING("[MANUAL] Dead-man cleared. Coil still off - press 's' to restore.\n");
            }

            HandleKey(static_cast<char>(key));
        }

        if (!m_manual)
        {
            return;
        }

        if ((WPT_MANUAL_DEADMAN_MS == 0) || m_deadman_tripped)
        {
            return;
        }

        m_idle_ms += WPT_MANUAL_POLL_PERIOD_MS;

        if (m_idle_ms >= WPT_MANUAL_DEADMAN_MS)
        {
            m_deadman_tripped = true;
            LOG_ERROR("[MANUAL] Dead-man timeout after %d ms with no input - pausing coil\n",
                      WPT_MANUAL_DEADMAN_MS);
            StopCoil();
        }
    }

    void DebugConsole::HandleKey(char key)
    {
        // Mode switching is always available. Everything else is ignored outside
        // manual mode, so a stray byte on the RTT channel cannot disturb a normal
        // automatic session.
        if (key == 'm')
        {
            EnterManual();
            return;
        }

        if (key == 'n')
        {
            ExitManual();
            return;
        }

        if (key == '?')
        {
            PrintStatus();
            return;
        }

        if (key == 'h')
        {
            PrintHelp();
            return;
        }

        if (!m_manual)
        {
            LOG_WARNING("[MANUAL] Ignoring '%c' - not in manual mode. Press 'm' first.\n", key);
            return;
        }

        switch (key)
        {
        case '+':
        case '=': // same physical key unshifted, accepted for convenience
            StepLevel(1);
            break;

        case '-':
            StepLevel(-1);
            break;

        case 's':
            StartCoil();
            break;

        case 'x':
            StopCoil();
            break;

        default:
            // PTH levels 0-12 as 0-9 then a, b, c.
            if ((key >= '0') && (key <= '9'))
            {
                SetLevel(static_cast<uint8_t>(key - '0'));
            }
            else if ((key >= 'a') && (key <= 'c'))
            {
                SetLevel(static_cast<uint8_t>(10 + (key - 'a')));
            }
            else
            {
                LOG_WARNING("[MANUAL] Unknown command '%c'. Press 'h' for help.\n", key);
            }
            break;
        }
    }

    void DebugConsole::EnterManual()
    {
        if (m_manual)
        {
            LOG_WARNING("[MANUAL] Already in manual mode\n");
            return;
        }

        // Only a request. OnManualEntered(), called from StateManual::Entry(), is
        // what sets m_manual - so a takeover that is dropped (it is not handled
        // during StateInitialization) leaves the console in AUTO, not confused.
        app::SystemPort::SendEvent(app::SystemPort::Event_e::MANUAL_TAKEOVER, 0);
        LOG_WARNING("[MANUAL] Manual takeover requested\n");
    }

    void DebugConsole::ExitManual()
    {
        if (!m_manual)
        {
            LOG_WARNING("[MANUAL] Not in manual mode\n");
            return;
        }

        // StateManual::Exit() clears m_manual via OnManualExited(), then stops the
        // fault timer, sends WPT_POWER_OFF, stops scanning and drops VCC_EN.
        app::SystemPort::SendEvent(app::SystemPort::Event_e::TURN_OFF, 0);
        LOG_WARNING("[MANUAL] Exit requested\n");
    }

    void DebugConsole::OnManualEntered()
    {
        m_manual = true;
        m_idle_ms = 0;
        m_deadman_tripped = false;

        LOG_WARNING("=====================================================\n");
        LOG_WARNING("[MANUAL] MANUAL CONTROL ENABLED\n");
        LOG_WARNING("[MANUAL] Any charging stopped. Coil OFF until 's'.\n");
        LOG_WARNING("[MANUAL] Thermal and OVP are WARN-ONLY. Button 1 disabled.\n");
        LOG_WARNING("=====================================================\n");

        PrintHelp();
    }

    void DebugConsole::OnManualExited()
    {
        m_manual = false;
        m_idle_ms = 0;
        m_deadman_tripped = false;

        LOG_WARNING("[MANUAL] Manual control DISABLED - coil off, rail down, back to StateWait\n");
        LOG_WARNING("[MANUAL] Press Button 1 for a normal automatic session.\n");
    }

    void DebugConsole::SetLevel(uint8_t level)
    {
        const uint8_t max_level = WptManager::GetMaxPowerLevel();

        if (level > max_level)
        {
            LOG_WARNING("[MANUAL] Level %d above maximum %d, clamping\n", level, max_level);
            level = max_level;
        }

        WptManager::SetPowerLevelManual(level);

        LOG_WARNING("[MANUAL] PTH level %d (%d mV)\n",
                    level, hal::Wpt_LTC4125::StepToMillivolts(level));
    }

    void DebugConsole::StepLevel(int8_t delta)
    {
        const uint8_t current = WptManager::GetPowerLevel();
        const uint8_t max_level = WptManager::GetMaxPowerLevel();

        if ((delta < 0) && (current == 0))
        {
            LOG_WARNING("[MANUAL] Already at minimum level 0\n");
            return;
        }

        if ((delta > 0) && (current >= max_level))
        {
            LOG_WARNING("[MANUAL] Already at maximum level %d\n", max_level);
            return;
        }

        SetLevel(static_cast<uint8_t>(current + delta));
    }

    void DebugConsole::StartCoil()
    {
        // Order matters. WPT StateIdle silently drops WPT_FAULT_RESUME and
        // WPT_ADJUST_POWER (its default case), so WPT_POWER_ON has to go first to
        // move the WPT state machine into StateCharging where the other two are
        // actually handled. Sending the resume first was the original bug: the
        // pause mask could never be cleared and EnableWpt() refused on it.
        //
        // PMC_POWER_ON is insurance only - StateManual::Entry() already raised the
        // rail, and it is a no-op while PMC is in StateEnable.
        svc::PmcPort::SendEvent(svc::PmcPort::Event_e::PMC_POWER_ON, 0);

        WptPort::SendEvent(WptPort::Event_e::WPT_POWER_ON, 0);
        WptPort::SendEvent(WptPort::Event_e::WPT_FAULT_RESUME,
                           static_cast<uint32_t>(WptManager::PAUSE_MANUAL));
        WptPort::SendEvent(WptPort::Event_e::WPT_ADJUST_POWER,
                           static_cast<uint32_t>(WptManager::GetPowerLevel()));

        LOG_WARNING("[MANUAL] Coil START requested at level %d (%d mV)\n",
                    WptManager::GetPowerLevel(),
                    hal::Wpt_LTC4125::StepToMillivolts(WptManager::GetPowerLevel()));
    }

    void DebugConsole::StopCoil()
    {
        // WPT_FAULT_PAUSE, not WPT_POWER_OFF: power-off transitions the WPT state
        // machine to StateIdle and calls DisableWpt() -> ResetPowerControl(), which
        // would tear down the very state being observed. A pause stops the coil and
        // leaves everything else standing.
        WptPort::SendEvent(WptPort::Event_e::WPT_FAULT_PAUSE,
                           static_cast<uint32_t>(WptManager::PAUSE_MANUAL));

        LOG_WARNING("[MANUAL] Coil STOP requested\n");
    }

    void DebugConsole::LogIpgBits()
    {
        if (svc::BleManager::GetAdvertisementCount() == 0)
        {
            LOG_WARNING("[MANUAL] IPG bits: no advertisement received yet\n");
            return;
        }

        const svc::ChargingStatusParameters_t &p =
            svc::BleManager::GetAdvertisementData().chargingStatusParameters;

        // Printed every fault tick in manual mode. When the coil is commanded on
        // but PGOOD is 0, this line is what separates "the IPG shut itself down"
        // from "the command did not take effect".
        LOG_WARNING("[MANUAL] IPG bits: VRECT_DETn=%d VRECT_OVPn=%d PGOOD=%d CHG1=%d CHG2=%d\n",
                    p.GET_VRECT_DET,
                    p.GET_VRECT_OVP,
                    p.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD,
                    p.GET_CHG1_STATUS,
                    p.GET_CHG2_STATUS);
    }

    void DebugConsole::PrintHelp()
    {
        LOG_WARNING("[MANUAL] m/n enter/exit manual   s/x coil start/stop\n");
        LOG_WARNING("[MANUAL] +/- step PTH            0-9,a,b,c set PTH 0-12\n");
        LOG_WARNING("[MANUAL] ?   status              h   this help\n");
    }

    void DebugConsole::PrintStatus()
    {
        const uint8_t level = WptManager::GetPowerLevel();
        const uint8_t reasons = WptManager::GetPauseReasons();

        LOG_WARNING("[MANUAL] --- status ---\n");
        LOG_WARNING("[MANUAL] mode=%s coil=%s pause_mask=0x%02X app=%s\n",
                    m_manual ? "MANUAL" : "AUTO",
                    WptManager::IsCoilEnabled() ? "ON" : "OFF",
                    reasons,
                    app::System::GetInstance().mSystemStateMachine.GetCurrentStateName());
        LOG_WARNING("[MANUAL] PTH level=%d (%d mV) max=%d\n",
                    level,
                    hal::Wpt_LTC4125::StepToMillivolts(level),
                    WptManager::GetMaxPowerLevel());
        LOG_WARNING("[MANUAL] ovp_ceiling=%d pgood_floor=%d floor_found=%d adv_count=%d\n",
                    WptManager::GetOvpCeiling(),
                    WptManager::GetPgoodFloor(),
                    WptManager::IsFloorFound(),
                    svc::BleManager::GetAdvertisementCount());

        LogIpgBits();

        if ((WPT_MANUAL_DEADMAN_MS != 0) && m_manual)
        {
            const uint32_t remaining = (m_idle_ms >= WPT_MANUAL_DEADMAN_MS)
                                           ? 0
                                           : (WPT_MANUAL_DEADMAN_MS - m_idle_ms);
            LOG_WARNING("[MANUAL] dead-man: %d s remaining\n", remaining / 1000);
        }
    }
}

#endif // WPT_MANUAL_DEBUG_MODE
