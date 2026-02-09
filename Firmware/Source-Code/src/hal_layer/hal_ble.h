/**
 * @name Hornet / WPT Charger
 * @file hal_ble.h
 * @brief BLE Hardware Abstraction Layer for nRF52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_BLE_H
#define HAL_BLE_H

#include "nrf_ble_scan.h"
#include "ble_gap.h"
#include "../../core_layer/event_driven_architecture/manager/eda_manager_log_config.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "app_error.h"

#define BLE_ADV_REPORT_MAX_SIZE 32

namespace hal
{

    /// Enumeration of BLE events
    typedef enum
    {
        BLE_SCAN_EVENT_FILTER_MATCH,
        BLE_SCAN_EVENT_WHITELIST_REQUEST,
        BLE_SCAN_EVENT_WHITELIST_ADV_REPORT,
        BLE_SCAN_EVENT_NOT_FOUND,
        BLE_SCAN_EVENT_SCAN_TIMEOUT,
        BLE_SCAN_EVENT_CONNECTING_ERROR,
        BLE_SCAN_EVENT_CONNECTED
    } BleScanEventType_e;

    /// Wrapper for ble_evt_t
    typedef struct
    {
        uint8_t active;
        uint8_t filter_policy;
        uint16_t interval;
        uint16_t window;
        uint16_t timeout;
    } BleGapScanParams_t;

    /// Wrapper for ble_data_t
    typedef struct
    {
        uint8_t *p_data; // Pointer to the data.
        uint16_t len;    // Length of the data.
    } BleGapData_t;

    /// Wrapper for ble_gap_evt_adv_report_t (focusing only on the data field)
    typedef struct
    {
        BleGapData_t data; // Received advertising or scan response data.
    } BleGapEventAdvReport_t;

    /// Wrapper for scan_evt_t
    typedef struct
    {
        BleScanEventType_e scan_evt_id;            ///< Type of event propagated to the main application.
        BleGapEventAdvReport_t *p_not_found; ///< Advertising report event parameters.
    } BleScanEvent_t;

    using EventHandler_t = void(ble_evt_t const *event, void *context);
    using ScanEventHandler_t = void (*)(BleScanEvent_t *event);

    class Ble
    {
    public:
        /// Constructor of the BLE class.
        Ble();

        /// Initialize the BLE module.
        static void Init(ScanEventHandler_t ScanEventHandler);

        /// Start scanning for BLE devices.
        static void StartScanning(void);

        /// Stop scanning for BLE devices.
        static void StopScanning(void);

    private:
        /// Initialize the BLE stack.
        static void BleStackInit(void);

        /// Initialize the BLE GAP parameters.
        static void GapParametersInit(void);

        /// Initialize the BLE scanning parameters.
        static void ScanningInit(void);

        /// Callback function for the BLE scanning events.
        ///
        /// @param p_scan_evt The scanning event.
        static void ScanEventCallback(scan_evt_t const *p_scan_evt);

        static EventHandler_t mBleEventHandler;

        static ScanEventHandler_t mScanEventHandler;

        static ble_gap_scan_params_t mScanParams;

        static BleScanEvent_t mScanEvent;

        static BleGapEventAdvReport_t mNotFoundData;

        static uint8_t mNotFoundDataBuffer[BLE_ADV_REPORT_MAX_SIZE]; // Adjust the size based on the max adv report size
    };
} // namespace hal

#endif // HAL_BLE_H