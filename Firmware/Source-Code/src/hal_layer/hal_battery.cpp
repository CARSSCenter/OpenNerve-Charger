/**
 * @name Hornet / WPT Charger
 * @file hal_battery.cpp
 * @brief Battery Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "hal_battery.h"

#include "hal_adc.h"
#include "hal_timer.h"
#include "svc_pmc_manager.h"
#include "hal_pinout.h"

#include "../core_layer/event_driven_architecture/manager/eda_manager_log_config.h"

namespace hal
{
    //Battery::Battery(HalBatteryEventHandler_t handler)
    Battery::Battery()
        : mTimerBattery(timerBatteryConfig, nullptr, this),
          //m_callback_measurement_ready(handler)
          m_callback_measurement_ready()
    {
    }

    void Battery::Start()
    {
        LOG_INFO("Battery measurement started\n");

        static hal::Adc::AdcChannel channel = {
          .pin = PIN_BAT_MEAS,
        };

        static int16_t batt_meas_voltage = 0;

        hal::Adc& m_adc = hal::Adc::get_instance();

        m_adc.take_measurement(&channel, &batt_meas_voltage);

        svc::PmcManager &pmcManager = svc::PmcManager::Instance();
        pmcManager.mBatteryVoltage =  static_cast<int16_t>((BATTERY_MEASUREMENT_GAIN_COEFFICIENT * static_cast<int32_t>(batt_meas_voltage)) / 1000);
    }

    void Battery::Stop()
    {
        // TODO delete this.
    }

    //float Battery::BatteryReadLastValue() const
    //{
    //    return mBatteryVoltage;
    //}

    void Battery::UnInit()
    {
        // TODO delete this.
    }

    void Battery::BatteryAdcCallback(void const *context)
    {
        //const Battery *instance = static_cast<const Battery *>(context);

        //// Calculate average of ADC samples
        //float sum = 0;
        //for (int i = 0; i < BATTERY_MEASUREMENT_BUFFER_SIZE; ++i)
        //{
        //    sum += instance->mBuffer[i];
        //}
        //float average = sum / BATTERY_MEASUREMENT_BUFFER_SIZE;

        //const_cast<Battery *>(instance)->mBatteryVoltage = (average);

        //LOG_INFO("Battery voltage %dmV", average);
        //if (const_cast<Battery *>(instance)->m_callback_measurement_ready)
        //{
        //  const_cast<Battery *>(instance)->m_callback_measurement_ready(average);
        //}
        //const_cast<Battery *>(instance)->Stop();
    }

    void Battery::GetBatteryVoltage()
    {
        Start();
    }
}
