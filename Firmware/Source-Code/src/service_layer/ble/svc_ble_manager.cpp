/**
 * @name Hornet / WPT Charger
 * @file svc_ble_manager.cpp
 * @brief BLE Service Layer Manager
 *
 * @copyright Copyright (c) 2024
 */

#include "../../core_layer/event_driven_architecture/manager/eda_manager_log_config.h"
#include "svc_ble_manager.h"
#include "svc_ble_port.h"

#include "app_error.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"

namespace svc
{
    AdvertisementData_t BleManager::mAdvertisementData;

    eda::Timer BleManager::mTimeoutTimer(TIMER_NAME, SCAN_TIMEOUT_MS, ONESHOT, TimerCallback);

    BleManager::BleManager(void)
    {
    }

    void BleManager::Init()
    {
        hal::Ble::Init(ScanEventHandler);
    }

    void BleManager::StartScanning(void)
    {
        LOG_DEBUG("Starting scan.");
        hal::Ble::StartScanning();
        StartTimeoutTimer();
    }

    void BleManager::StopScanning(void)
    {
        LOG_DEBUG("Stopping scan.");

        hal::Ble::StopScanning();
        StopTimeoutTimer();
    }

    void BleManager::StartTimeoutTimer(void)
    {
        LOG_DEBUG("Starting timeout timer.");
        mTimeoutTimer.Start();
    }

    void BleManager::StopTimeoutTimer(void)
    {
        LOG_DEBUG("Stopping timeout timer.");
        mTimeoutTimer.Stop();
    }

    void BleManager::TimerCallback(TimerHandle_t xTimer)
    {
        LOG_DEBUG("Timeout timer expired.");
        mTimeoutTimer.StopFromISR();
        BlePort::SendEvent(BlePort::Event_e::SCAN_TIMEOUT, NULL);
    }

    void BleManager::ScanEventHandler(hal::BleScanEvent_t *event)
    {
        switch (event->scan_evt_id)
        {
        case NRF_BLE_SCAN_EVT_SCAN_TIMEOUT:
            LOG_DEBUG("Scan timed out.");
            BlePort::SendEvent(BlePort::Event_e::SCAN_TIMEOUT, NULL);
            break;
        case NRF_BLE_SCAN_EVT_FILTER_MATCH:
            LOG_DEBUG("Filter match.");
            break;
        case NRF_BLE_SCAN_EVT_WHITELIST_ADV_REPORT:
            LOG_DEBUG("Whitelist adv report.");
            break;
        case NRF_BLE_SCAN_EVT_NOT_FOUND:
            AdvertisementEventHandler(event->p_not_found);
            break;
        default:
            break;
        }
    }

    void BleManager::AdvertisementEventHandler(const hal::BleGapEventAdvReport_t *p_adv_report)
    {
        uint8_t *adv_data = p_adv_report->data.p_data;
        uint8_t adv_data_len = p_adv_report->data.len;

        ParseAdvertisementData(adv_data, adv_data_len);
    }

    void BleManager::ParseAdvertisementData(uint8_t *adv_data, uint16_t adv_data_len)
    {
        uint16_t index = 0;

        while (index < adv_data_len)
        {
            uint8_t length = adv_data[index];
            if (length == 0 || (index + length >= adv_data_len))
            {
                break;
            }

            uint8_t type = adv_data[index + 1];
            uint8_t *data = &adv_data[index + 2];
            uint8_t data_length = length - 1;

            // Handle the type and data accordingly
            switch (type)
            {
            case BLE_GAP_AD_TYPE_FLAGS:
                // Handle flags
                break;
            case BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME:
                // Handle complete local name
                break;
            case BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA:
                // Handle manufacturer-specific data
                ParseManufacturerSpecificData(adv_data, adv_data_len, index);
                break;
            // Add other cases as needed
            default:
                break;
            }

            // Move to the next field
            index += length + 1;
        }
    }

    void BleManager::ParseManufacturerSpecificData(uint8_t *adv_data, uint16_t adv_data_len, uint16_t index)
    {
        // Check if this is the manufacturer-specific data with Company ID 0xFFFF
        uint16_t company_id = adv_data[index + 2] | (adv_data[index + 3] << 8);

        if (company_id == CARSS_COMPANY_ID)
        {
            mAdvertisementData.chargingStatusParameters.GET_VRECT_DET = adv_data[index + 4];
            mAdvertisementData.chargingStatusParameters.GET_VRECT_OVP = adv_data[index + 5];
            mAdvertisementData.chargingStatusParameters.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD = adv_data[index + 6];
            mAdvertisementData.chargingStatusParameters.GET_CHG1_STATUS = adv_data[index + 7];
            mAdvertisementData.chargingStatusParameters.GET_CHG1_OVP_ERR = adv_data[index + 8];
            mAdvertisementData.chargingStatusParameters.GET_CHG2_STATUS = adv_data[index + 9];
            mAdvertisementData.chargingStatusParameters.GET_CHG2_OVP_ERR = adv_data[index + 10];
            mAdvertisementData.chargingStatusParameters.GET_THERM_REF = adv_data[index + 11] | (adv_data[index + 12] << 8);
            mAdvertisementData.chargingStatusParameters.GET_THERM_OUT = adv_data[index + 13] | (adv_data[index + 14] << 8);
            mAdvertisementData.chargingStatusParameters.GET_THERM_OFST = adv_data[index + 15] | (adv_data[index + 16] << 8);
            mAdvertisementData.chargingStatusParameters.BATTERY_VOLTAGE_MEASURED = adv_data[index + 17] | (adv_data[index + 18] << 8) | (adv_data[index + 19] << 16) | (adv_data[index + 20] << 24);
            mAdvertisementData.chargingStatusParameters.GET_TEST_INFO = adv_data[index + 21] | (adv_data[index + 22] << 8) | (adv_data[index + 23] << 16);

            uint32_t optDataAddress = reinterpret_cast<uint32_t>(&mAdvertisementData);

            BlePort::SendEventFromISR(BlePort::Event_e::DEVICE_FOUND, optDataAddress);

            // Restart the timer when the Device is found
            mTimeoutTimer.StartFromISR();
        }
    }

    void BleManager::ParseLocalName(uint8_t *adv_data, uint16_t adv_data_len, uint16_t index, uint8_t length)
    {
        uint8_t name_len = length - 1;

        if (name_len > sizeof(mAdvertisementData.localName) - 1)
        {
            name_len = sizeof(mAdvertisementData.localName) - 1;
        }

        memcpy(mAdvertisementData.localName, &adv_data[index + 2], name_len);

        LOG_DEBUG("Local Name: %s", nrf_log_push(mAdvertisementData.localName));
    }

}