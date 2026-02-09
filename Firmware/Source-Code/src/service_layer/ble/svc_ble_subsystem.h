/**
 * @name Hornet / WPT Charger
 * @file svc_ble_subsystem.h
 * @brief BLE Service Layer Subsystem
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_ble_port.h"
#include "state_machine/svc_ble_state_machine.h"
#include "../../core_layer/event_driven_architecture/active_object/eda_active_object.h"

namespace svc
{

    class BleSubsystem
    {
        friend BlePort;

    public:
        static BleSubsystem & Instance();

        void Init();

        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);

        BlePort mBlePort;

    private:
        BleSubsystem();

        eda::ActiveObject mBleActiveObject;

        BleStateMachine mBleStateMachine;
    };

}
