/**
 * @name Hornet / WPT Charger
 * @file svc_ble_manager.h
 * @brief BLE Service Layer Manager
 *
 * @copyright Copyright (c) 2024
 */

#ifndef SVC_BLE_MANAGER_H
#define SVC_BLE_MANAGER_H

#include "hal_ble.h"

#include "eda_timer.h"

#include <cstdint>

#define SCAN_INTERVAL 0x00A0
#define SCAN_WINDOW 0x0050

#define TIMER_NAME "BleTimeoutTimer"
#define SCAN_TIMEOUT_MS 10000 //Changed from original value of 2000
#define PERIODIC 1
#define ONESHOT 0

#define APP_BLE_OBSERVER_PRIO 3
#define APP_BLE_CONN_CFG_TAG 1

// The CARSS_COMPANY_ID on the IPG is 0xF0F0 for production firmware, 0xFFFF for DVT firmware
#define CARSS_COMPANY_ID 0xF0F0

namespace svc
{
    enum class BleEventType_e : uint8_t
    {
        INVALID,
        SCANNING_STARTED,
        STOP_SCANNING
    };

    enum class BleManagerStatus_e : uint8_t
    {
        INVALID,
        IDLE,
        SCANNING
    };

    typedef struct
    {
        uint8_t GET_VRECT_DET;
        uint8_t GET_VRECT_OVP;
        uint8_t GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD;
        uint8_t GET_CHG1_STATUS;
        uint8_t GET_CHG1_OVP_ERR;
        uint8_t GET_CHG2_STATUS;
        uint8_t GET_CHG2_OVP_ERR;
        uint16_t GET_THERM_REF;
        uint16_t GET_THERM_OUT;
        uint16_t GET_THERM_OFST;
        uint32_t BATTERY_VOLTAGE_MEASURED;
        uint32_t GET_TEST_INFO;
    } ChargingStatusParameters_t;

    typedef struct
    {
        ChargingStatusParameters_t chargingStatusParameters;
        char localName[32];
    } AdvertisementData_t;

    using EventHandler_t = void (*)(ble_evt_t *event, void *context);

    class BleManager
    {
    public:
        /// Constructor of the BLE Manager class
        BleManager();

        /// Initialize the BLE Manager
        static void Init();

        /// Start BLE Scanning
        static void StartScanning(void);

        /// Stop BLE Scanning
        static void StopScanning(void);

        // Getter for advertisement data
        static const AdvertisementData_t& GetAdvertisementData() 
        {
            return mAdvertisementData;
        }

    private:
        static EventHandler_t mEventHandler;

        static AdvertisementData_t mAdvertisementData;

        static eda::Timer mTimeoutTimer;

        /// Start the timeout timer
        static void StartTimeoutTimer(void);

        /// Stop the timeout timer
        static void StopTimeoutTimer(void);

        /// Callback function for the timer
        ///
        /// @param xTimer Handle to the timer
        static void TimerCallback(TimerHandle_t xTimer);

        /// Handle BLE scan events
        ///
        /// @param event Pointer to the BLE scan event
        static void ScanEventHandler(hal::BleScanEvent_t *event);

        /// Handle advertisement events
        ///
        /// @param p_adv_report Pointer to the advertisement report
        static void AdvertisementEventHandler(const hal::BleGapEventAdvReport_t *p_adv_report);

        /// Parse advertisement data
        ///
        /// @param adv_data Pointer to the advertisement data
        /// @param adv_data_len Length of the advertisement data
        static void ParseAdvertisementData(uint8_t *adv_data, uint16_t adv_data_len);

        /// Parse manufacturer-specific data from advertisement data
        ///
        /// @param adv_data Pointer to the advertisement data
        /// @param adv_data_len Length of the advertisement data
        /// @param index Index where the manufacturer-specific data starts
        static void ParseManufacturerSpecificData(uint8_t *adv_data, uint16_t adv_data_len, uint16_t index);

        /// Parse the local name from advertisement data
        ///
        /// @param adv_data Pointer to the advertisement data
        /// @param adv_data_len Length of the advertisement data
        /// @param index Index where the local name starts
        /// @param length Length of the local name
        static void ParseLocalName(uint8_t *adv_data, uint16_t adv_data_len, uint16_t index, uint8_t length);
    };
}

#endif