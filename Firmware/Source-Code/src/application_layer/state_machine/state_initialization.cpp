/**
 * @name Hornet / WPT Charger
 * @file state_initialization.cpp
 * @brief Implementation file for the initialization state
 *
 * @copyright Copyright (c) 2024
 */

#include "state_initialization.h"

#include "app_port.h"
#include "app_state_machine.h"
#include "eda_manager_log_config.h"
#include "hal_dfu.h"
#include "svc_ble_port.h"
#include "svc_ble_subsystem.h"
#include "svc_pmc_port.h"
#include "svc_pmc_subsystem.h"
#include "svc_wpt_port.h"
#include "svc_wpt_subsystem.h"

namespace app
{
    struct StatePointers;

    void StateInitialization::Entry()
    {
        LOG_INFO("App state machine: Initialization State Entry\n");
        
        svc::BleSubsystem& mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::INITIALIZE, NULL);

        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::INITIALIZE, NULL);

        svc::PmcSubSystem& mPmcSubsystem = svc::PmcSubSystem::Instance();
        mPmcSubsystem.mPmcPort.SendEvent(svc::PmcPort::Event_e::INITIALIZE, NULL);
        
    }

    void StateInitialization::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        SystemStateMachine *stateMachine = reinterpret_cast<SystemStateMachine *>(mStateMachine);

        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<SystemPort::Event_e>(eventId))
        {
        case SystemPort::Event_e::BLE_INITIALIZED:
            stateMachine->ChangeState(states->pStateWait);
            break;
        case SystemPort::Event_e::BUTTON_DFU_PRESSED:
            if (!hal::Dfu::Instance().is_dfu_active())
            {
                hal::Dfu::Instance().start_dfu_mode();
            }
            break;
        }
    }
    void StateInitialization::Exit()
    {
    }

} // namespace app