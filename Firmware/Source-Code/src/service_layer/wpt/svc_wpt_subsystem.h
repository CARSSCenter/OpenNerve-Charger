/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_subsystem.h
 * @brief WptSubsystem class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef WPT_SUBSYSTEM_H
#define WPT_SUBSYSTEM_H

#include "eda_active_object.h"
#include "svc_wpt_port.h"
#include "svc_wpt_state_machine.h"

namespace svc
{
    class WptSubsystem
    {
    public:
        static WptSubsystem &Instance();

        void Init();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);

        WptPort mWptPort;

    private:
        WptSubsystem();

        eda::ActiveObject mWptActiveObject;

        WptStateMachine mWptStateMachine;
    };
}
#endif // WPT_SUBSYSTEM_H