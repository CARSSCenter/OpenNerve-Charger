/**
 * @name Hornet / WPT Charger
 * @file eda_state_machine.cpp
 * @brief State Machine class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "eda_state_machine.h"
#include <cassert>
namespace eda
{
    void State::Entry()
    {
    }

    void State::Exit()
    {
    }


    void StateMachine::Init()
    {
        InitAction();
        if (NextStateIsSet())
        {
            mPreviousState = mNextState;
            mCurrentState = mNextState;
            mNextState = nullptr;
        }
        // If previous 
        // Define whether asserts are necessary here
        mCurrentState->Entry();
    }

    void StateMachine::InitAction()
    {

    }

    void StateMachine::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        if (mCurrentState != nullptr)
        {
            mCurrentState->DispatchEvent(eventId, optDataAddress);
        }
    }

    void StateMachine::ChangeState(State * newState)
    {
        mCurrentState->Exit();
        mPreviousState = mCurrentState;
        mCurrentState = newState;
        mCurrentState->Entry();
    }

    State* StateMachine::GetLastState()
    {
        return mPreviousState;
    }

    void StateMachine::ReturnToLastState()
    {
        State *current_state;
        mCurrentState->Exit();
        current_state = mCurrentState;
        mCurrentState = mPreviousState;
        mPreviousState = current_state;
        mCurrentState->Entry();
    }

    void StateMachine::ChangeToNextState()
    {
        mCurrentState->Exit();
        mPreviousState = mCurrentState;
        mCurrentState = mNextState;
        mNextState = nullptr;
        mCurrentState->Entry();
    }

    void StateMachine::SetNextState(State * nextState)
    {
        mNextState = nextState;
    }

    bool StateMachine::NextStateIsSet()
    {
        return static_cast<bool>(mNextState);
    }

    const char* StateMachine::GetCurrentStateName() const
    {
        return mCurrentState->mName;
    }
}