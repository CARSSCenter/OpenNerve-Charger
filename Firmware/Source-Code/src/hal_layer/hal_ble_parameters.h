/**
 * @name Hornet / WPT Charger
 * @file hal_ble_parameters.h
 * @brief Parameters for the BLE Hardware Abstraction Layer for nRF52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_BLE_PARAMETERS_H
#define HAL_BLE_PARAMETERS_H

#define SCAN_INTERVAL_MS 500
#define SCAN_WINDOW_MS 1000

#define APP_BLE_OBSERVER_PRIO 3
#define APP_BLE_CONN_CFG_TAG 1

#define ADV_TYPE_FLAGS 0x01
#define ADV_TYPE_SHORT_LOCAL_NAME 0x08
#define ADV_TYPE_COMPLETE_LOCAL_NAME 0x09
#define ADV_TYPE_MANUFACTURER_DATA 0xFF

#endif // HAL_BLE_PARAMETERS_H