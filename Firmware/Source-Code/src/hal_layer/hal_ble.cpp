/**
 * @name Hornet / WPT Charger
 * @file hal_ble.cpp
 * @brief BLE Hardware Abstraction Layer for nRF52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_ble.h"
#include "hal_ble_parameters.h"
#include "nrf_ble_scan.h"

NRF_BLE_SCAN_DEF(mScan);

namespace hal
{
    ScanEventHandler_t Ble::mScanEventHandler = nullptr;

    ble_gap_scan_params_t Ble::mScanParams;

    BleScanEvent_t Ble::mScanEvent;

    uint8_t Ble::mNotFoundDataBuffer[BLE_ADV_REPORT_MAX_SIZE] = {0}; // Initialize buffer

    Ble::Ble()
    {
    }

    void Ble::Init(ScanEventHandler_t ScanEventHandler)
    {
        mScanEventHandler = ScanEventHandler;

        BleStackInit();
        GapParametersInit();
        ScanningInit();
    }

    void Ble::StartScanning(void)
    {
        ret_code_t err_code;

        LOG_INFO("Starting scan.");

        err_code = nrf_ble_scan_start(&mScan);
        APP_ERROR_CHECK(err_code);
    }

    void Ble::StopScanning(void)
    {
        nrf_ble_scan_stop();
    }

    void Ble::BleStackInit()
    {
        ret_code_t err_code;

        err_code = nrf_sdh_enable_request();
        APP_ERROR_CHECK(err_code);

        uint32_t ram_start = 0;
        err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
        APP_ERROR_CHECK(err_code);

        err_code = nrf_sdh_ble_enable(&ram_start);
        APP_ERROR_CHECK(err_code);

        // The current implementation does not require a BLE event handler
        NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, mBleEventHandler, NULL);
    }

    void Ble::GapParametersInit(void)
    {
        mScanParams.interval = SCAN_INTERVAL_MS;
        mScanParams.window = SCAN_WINDOW_MS;
        mScanParams.timeout = 0;
        mScanParams.active = 0;
        mScanParams.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
    }

    void Ble::ScanningInit()
    {
        ret_code_t err_code;
        nrf_ble_scan_init_t scan_init;

        memset(&scan_init, 0, sizeof(scan_init));

        scan_init.connect_if_match = false;
        scan_init.p_scan_param = &mScanParams;

        err_code = nrf_ble_scan_init(&mScan, &scan_init, ScanEventCallback);
        APP_ERROR_CHECK(err_code);
    }

    void Ble::ScanEventCallback(scan_evt_t const *p_scan_evt)
    {
        // Initialize the event structure
        mScanEvent.scan_evt_id = static_cast<BleScanEventType_e>(p_scan_evt->scan_evt_id);

        // Ensure that mScanEvent.p_not_found points to a valid memory location
        static BleGapEventAdvReport_t not_found_data; // Static allocation for the not_found structure
        mScanEvent.p_not_found = &not_found_data;

        // Check if the p_not_found field in the scan event is valid
        if (p_scan_evt->params.p_not_found != nullptr)
        {
            mScanEvent.p_not_found->data.p_data = p_scan_evt->params.p_not_found->data.p_data;
            mScanEvent.p_not_found->data.len = p_scan_evt->params.p_not_found->data.len;
        }
        else
        {
            // Handle the case where no data was found (initialize to safe values)
            mScanEvent.p_not_found->data.p_data = nullptr;
            mScanEvent.p_not_found->data.len = 0;
        }

        mScanEventHandler(&mScanEvent);
    }

    void Ble::mBleEventHandler(ble_evt_t const *event, void *context)
    {
        // No implementation required for now
    }

}