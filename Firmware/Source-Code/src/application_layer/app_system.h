/**
 * @name Hornet / WPT Charger
 * @file app_system.h
 * @brief System class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

#include "../core_layer/event_driven_architecture/active_object/eda_active_object.h"
#include "app_port.h"
#include "state_machine/app_state_machine.h"  
#include "eda_timer.h"  


namespace app
{
    /// The System class represents the application system.
    ///
    /// This class provides functionality to initialize and run the application system.
    ///
    class System
    {
    public:
        /// Deleted copy constructor.
        ///
        System(const System &) = delete;

        /// Deleted copy assignment operator.
        ///
        System &operator=(const System &) = delete;

        /// Get the instance of the System class.
        static System &GetInstance();

        /// Initialize the application system.
        ///
        void Init();

        /// Run the application system.
        ///
        void Run();

        eda::ActiveObject mSystemActiveObject;

        SystemPort mSystemPort;
        SystemStateMachine mSystemStateMachine;

    private:
        /// Private constructor of the System class.
        System();
        eda::Timer mHeartbeatTimer;

        static void timerCallback(TimerHandle_t xTimer);
        
    };
};

#endif