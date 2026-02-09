/**
 * @name Hornet / WPT Charger
 * @file app_system.cpp
 * @brief System class declaration
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "app_system.h"

#include "../project/version.h"
#include "eda_active_object.h"
#include "eda_active_object_priorities.h"
#include "eda_manager.h"
#include "svc_ble_subsystem.h"
#include "svc_pmc_manager.h"
#include "svc_wpt_manager.h"
#include "svc_pmc_subsystem.h"
#include "svc_wpt_subsystem.h"

#include <cstdint>

namespace app
{
    System::System() : mSystemStateMachine(), mHeartbeatTimer("HeartbeatTimer", 5000, 1, timerCallback)
    {
    }

    System &System::GetInstance()
    {
        static System instance;
        return instance;
    }

    void System::Init()
    {
        eda::Manager::Initialize();

        mSystemActiveObject.InitTask(eda::ActiveObjectPriorities_e::app, "SystemActiveObject");
        mSystemPort.Init(PortList_e::SYSTEM_PORT, mSystemActiveObject);

        // Init subsystems
        svc::PmcSubSystem& mPmcSubsystem = svc::PmcSubSystem::Instance();
        mPmcSubsystem.Init();
        
        svc::WptSubsystem& mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.Init();

        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.Init();

        mSystemStateMachine.Init();

        mHeartbeatTimer.Start();

        LOG_INFO("System Initialized - Version %d.%d.%d (%s)\n", VER_MAJOR, VER_MINOR, VER_REVISION, VER_ID);
    }

    void System::Run()
    {
        eda::Manager::StartEventDrivenArchitecture();
    }

    void System::timerCallback(TimerHandle_t xTimer)
    {
        static uint32_t heartbeat = 0;

        heartbeat += 1;
        LOG_INFO("Heartbeat value %d\n", heartbeat); 

        svc::WptManager &wptManager = svc::WptManager::Instance();
        wptManager.GetTemperature();
        LOG_INFO("WPT Charger NTC Temperature %d C\n", wptManager.mWptNtcTemperature);

        svc::PmcManager &pmcManager = svc::PmcManager::Instance();
        pmcManager.GetBatteryVoltage();
        LOG_INFO("Battery voltage level %d mV\n", pmcManager.mBatteryVoltage);
    }
}
