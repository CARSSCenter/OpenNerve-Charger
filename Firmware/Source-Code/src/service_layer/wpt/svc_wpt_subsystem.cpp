/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_subsystem.cpp
 * @brief WptSubsystem class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_wpt_subsystem.h"

#include "../../core_layer/event_driven_architecture/active_object/eda_active_object_priorities.h"

namespace svc
{
    WptSubsystem &WptSubsystem::Instance()
    {
        static WptSubsystem instance;
        return instance;
    }

    WptSubsystem::WptSubsystem() : mWptActiveObject(), mWptPort(), mWptStateMachine() {}

    void WptSubsystem::Init()
    {
        mWptActiveObject.InitTask(eda::ActiveObjectPriorities_e::svc_1, "WPT");
        mWptPort.Init(app::PortList_e::WPT_PORT, mWptActiveObject);
        mWptStateMachine.Init();
    }

    void WptSubsystem::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        mWptStateMachine.DispatchEvent(eventId, optDataAddress);
    }

}