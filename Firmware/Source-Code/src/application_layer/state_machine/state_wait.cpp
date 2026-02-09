/**
 * @name Hornet / WPT Charger
 * @file state_wait.cpp
 * @brief Implementation file for the wait state
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_wait.h"

#include "app_port.h"
#include "app_state_machine.h"
#include "hal_dfu.h"
#include "hal_led.h"


namespace app
{
    struct StatePointers;

    void StateWait::Entry()
    {   
        //Do nothing
    }   

    void StateWait::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        SystemStateMachine *stateMachine = reinterpret_cast<SystemStateMachine *>(mStateMachine);

        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<SystemPort::Event_e>(eventId))
        {
        case SystemPort::Event_e::BUTTON_PRESSED:
            stateMachine->ChangeState(states->pStateScan);
            break;
        case SystemPort::Event_e::BUTTON_DFU_PRESSED:
            if (!hal::Dfu::Instance().is_dfu_active())
            {
                hal::Dfu::Instance().start_dfu_mode();
            }
            break;
        }
    }

    void StateWait::Exit()
    {
        hal::Leds& leds = hal::Leds::GetInstance();
        leds.TurnLedOff(&leds.rgb_led);
    }
} // namespace app