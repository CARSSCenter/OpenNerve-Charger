/**
 * @name Hornet / WPT Charger
 * @file hal_battery.h
 * @brief Battery Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_BATTERY_H
#define HAL_BATTERY_H

#include "hal_adc.h"
#include "hal_timer.h"
#include "hal_pinout.h"

#define BATTERY_TIMER TIMER_1
#define BATTERY_PPI_CHANNEL NRF_PPI_CHANNEL0
#define BATTERY_ADC_CHANNEL 0

namespace hal
{
  static constexpr uint8_t BATTERY_MEASUREMENT_BUFFER_SIZE = 16;
  static constexpr uint32_t BATTERY_TIMER_PERIOD = 156250; /// 20msecs
                                                           /// Time interval between timer events, in nanoseconds. It should be a multiple of 128ns
  using HalBatteryEventHandler_t = void(float);

  /// Battery HAL implementation.
  class Battery
  {
  public:
    //Battery(HalBatteryEventHandler_t handler);
    Battery();

    /// Uninitialize the battery hal.
    void UnInit(void);

    /// Read last valid value obtain
    float BatteryReadLastValue(void) const;

    /// Obtain a current battery value
    void GetBatteryVoltage(void);

  private:
    void Start(void);
    void Stop(void);
    static void BatteryAdcCallback(void const *context);

    HalBatteryEventHandler_t *m_callback_measurement_ready;

    static constexpr Timer::Config_t timerBatteryConfig = {
        .period = BATTERY_TIMER_PERIOD,
        .autostart = true,
        .timerNumber = Timer::PeripheralNumber::BATTERY_TIMER};

    Timer mTimerBattery;

    int16_t mBuffer[BATTERY_MEASUREMENT_BUFFER_SIZE] = {0};

    static constexpr int32_t BATTERY_MEASUREMENT_GAIN_COEFFICIENT = 1300; // 1.3*1000
  };

}

#endif
