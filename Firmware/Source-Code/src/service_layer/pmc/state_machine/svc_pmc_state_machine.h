/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_state_machine.h
 * @brief PmcStateMachine class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef PMC_STATE_MACHINE_H
#define PMC_STATE_MACHINE_H

#include "eda_state_machine.h"

#include "svc_pmc_manager.h"
#include "svc_pmc_port.h"

#include "svc_pmc_state_enable.h"
#include "svc_pmc_state_idle.h"
#include "svc_pmc_state_charging_battery.h"

namespace svc
{
    struct PmcStatePointers
    {
        PmcStateIdle *pStateIdle;
        PmcStateEnable *pStateEnable;
        PmcStateChargingBattery *pStateChargingBattery;
    };

    class PmcStateMachine : public eda::StateMachine
    {
    public:
        PmcStateMachine();
        PmcStatePointers *GetStates();

    private:
        PmcStateIdle mStateIdle;
        PmcStateEnable mStateEnable;
        PmcStateChargingBattery mStateChargingBattery;
    };
} // namespace svc

#endif // PMC_STATE_MACHINE_H