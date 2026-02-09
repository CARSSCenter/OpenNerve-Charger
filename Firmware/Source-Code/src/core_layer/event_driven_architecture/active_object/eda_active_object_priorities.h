/**
 * @name Hornet / WPT Charger
 * @file eda_active_object_priorities.h
 * @brief Active Object Priorities Enum class declaration
 *
 * @copyright Copyright (c) 2024
 *
 */


#ifndef EDA_ACTIVE_OBJECT_PRIORITIES_H
#define EDA_ACTIVE_OBJECT_PRIORITIES_H

#include <cstdint>

namespace eda
{
    enum class ActiveObjectPriorities_e : uint8_t
    {
        // TODO:
        // Define accurrately these priorities 
        idle,
        svc_1,
        svc_2,
        svc_3,
        app,
        sd_ble_task,
    };
}

#endif