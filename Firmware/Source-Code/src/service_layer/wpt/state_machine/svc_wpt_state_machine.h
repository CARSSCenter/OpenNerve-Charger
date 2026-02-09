/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_machine.h
 * @brief WptStateMachine class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef WPT_STATE_MACHINE_H
#define WPT_STATE_MACHINE_H

#include "../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"
#include "svc_wpt_port.h"
#include "svc_wpt_manager.h"
#include "svc_wpt_state_idle.h"
#include "svc_wpt_state_test.h"
#include "svc_wpt_state_slow_charge.h"
#include "svc_wpt_state_charging.h"

namespace svc
{
    struct StatePointers
    {
        StateIdle *pStateIdle;
        StateTest *pStateTest;
        StateSlowCharge *pStateSlowCharge;
        StateCharging *pStateCharging;
    };

    class WptStateMachine : public eda::StateMachine
    {
    public:
        WptStateMachine();
        StatePointers *GetStates();

    private:
        StateIdle mStateIdle;
        StateTest mStateTest;
        StateSlowCharge mStateSlowCharge;
        StateCharging mStateCharging;
    };
} // namespace svc

#endif // WPT_STATE_MACHINE_H