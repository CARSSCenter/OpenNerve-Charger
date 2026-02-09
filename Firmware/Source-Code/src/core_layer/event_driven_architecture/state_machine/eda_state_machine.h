/**
 * @name Hornet / WPT Charger
 * @file eda_state_machine.cpp
 * @brief State Machine class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef EDA_STATE_MACHINE_H
#define EDA_STATE_MACHINE_H

namespace eda
{
    class StateMachine;
};

#include <cstdint>

namespace eda
{
    /// A StateMachine is compromised of at least 2 State objects (there is no
    /// need for a StateMachine with only 1 State). The State defines the actions
    /// and behaviors the are performed when StateMachine runs a State.
    class State
    {
        friend StateMachine;

    public:
        /// @brief Constructor sets the state name used for logging.
        /// @param name A short string used to name the state.
        State(const char *name, StateMachine *stateMachine) : mName(name), mStateMachine(stateMachine)
        {
        }

    protected:
        StateMachine *mStateMachine;

    private:
        /// The Entry is optionally defined y a derived state.
        /// It is the action performed when the state is entered.
        virtual void Entry();

        /// The Exit is optionally defined y a derived state.
        /// It is the action performed when the state is exited.
        virtual void Exit();

        /// A derived class's DispatchEvent should tipically consist of a switch statement
        ///  for handling each event type within the state.
        virtual void DispatchEvent(uint32_t eventId, uint32_t optDataAddress) = 0;

        const char *mName;
    };

    /// StateMachine consist of an aggregation of States. The StateMachine
    /// tracks th ecurrently active State and performs state changes when requested.
    /// A derived StateMachine class should define its States and the initial State.
    class StateMachine
    {
    public:
        /// Constructor
        /// @param name A short string used to name the state machine in logging.
        /// @param initialState The initial State that the StateMachine should start in.

        StateMachine(const char *name, State *initialState) : mName(name), mCurrentState(initialState), mPreviousState(nullptr), mNextState(nullptr)
        {
        }

        /// @brief Perform the initial transition.
        void Init();

        /// The InitAction is optionally defined by a derived state.
        /// It is the action performed when the state machine is intialized prior to entry to the first state.
        /// If the initial state should be changed, this is the place to do so via calling SetNextState.
        virtual void InitAction();

        /// @brief  This routes an event to the current State's dispatcher.
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);

        /// @brief  Transition from one State to another, performing exit and entry actions.
        void ChangeState(State *newState);

        /// @brief  Gets the value of the last state from history.
        /// @return Pointerr to last state.
        State *GetLastState();

        /// @brief  Transition to the last state from history.
        ///         The last state is the previous state before the current one.
        void ReturnToLastState();

        /// @brief  Transition to the next state if set.
        void ChangeToNextState();

        /// @brief  Set next state. This enables storing of conditional logic for state transitions
        ///         if the associated event is not a transitional event.
        void SetNextState(State *newState);

        /// @brief  Check if we have a next state set. This way we know whether we can call ChangeToNextState().
        /// @return Booolean, true if next state is setted.
        bool NextStateIsSet();

        /// @brief  Get name of current state.
        /// @return Pointer to string containing name of current state.
        const char *GetCurrentStateName() const;

    private:
        State *mCurrentState;
        State *mPreviousState;
        State *mNextState;

        const char *mName;
    };
}
#endif
