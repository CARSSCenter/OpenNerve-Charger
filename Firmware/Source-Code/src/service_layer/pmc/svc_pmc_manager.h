/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_manager.h
 * @brief PmcManager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SVC_PMC_MANAGER_H
#define SVC_PMC_MANAGER_H

#include "svc_pmc_port.h"

#include "hal_battery.h"
#include "hal_gpio.h"
#include "hal_pinout.h"

namespace svc
{
    class PmcManager
    {
    public:
        static PmcManager &Instance();

        /// This method initialize or configurates features
        void Init();

        /// This method configures the GPIOs.
        void ConfigureGpios();

        /// This method enables the VCC regulator.
        void EnableVccRegulator();

        /// This method disables the VCC regulator.
        void DisableVccRegulator();

        /// This method enables the Battery Charge IC.
        void EnableBatteryCharger();

        /// This method gets the battery voltage level.
        void GetBatteryVoltage();

        /// This method disables the Battery Charge IC.
        void DisableBatteryCharger();

        /// Open-drain logic output to indicate the input power status.
        /// The PPR-pin output is only determined by the input voltage,
        /// not other conditions such as the EN pin input. The output is
        /// low if VIN is higher than VPOR. This pin is capable to sink at
        /// least 12.0mA current to drive a LED indicator.
        void ReadBatteryChargerPresentIndicator();

        /// Open-drain logic output to indicate the charge status. The
        /// output is low when the MC34673 is charging, until the EOC
        /// conditions are reached. This pin is capable to sink at least
        /// 12.0mA current to drive a LED indicator.
        void ReadBatteryChargeIndicator();

        /// When charging, this open-drain logic output indicates
        /// whether or not the battery voltage is higher than the trickle-ode threshold. 
        /// This pin is capable to sink more than 0.3mA current.
        /// When the charger is on, this pin outputs a logic low
        /// signal if the battery voltage is higher than the trickle-mode
        /// threshold. When the charger is in the shutdown mode or in
        /// any fault conditions, this pin outputs high-impedance.
        void ReadBatteryFastChargeIndicator();

        void ReadBatteryCurrentSettings();

        int16_t mBatteryVoltage;

    private:
        /// Construct PmcManager
        PmcManager();
        static constexpr uint32_t ENABLE_INACTIVE_LEVEL = 0;
        static constexpr uint32_t ENABLE_ACTIVE_LEVEL = 1;

        static constexpr uint32_t READ_INACTIVE_LEVEL = 0;
        static constexpr uint32_t READ_ACTIVE_LEVEL = 1;

        static void StaticReadBatteryChargerPresentIndicator();
        static void StaticReadBatteryChargeIndicator();
        static void StaticReadBatteryFastChargeIndicator();

        uint32_t mVccEnablePin = PIN_PMC_VCC_EN;
        uint32_t mChgEnablePin = PIN_CHG_EN;
        uint32_t mChrPwrPresentIndicatorPin = PIN_CHG_PPR;
        uint32_t mChgChargeIndicatorPin = PIN_CHG_CHG;
        uint32_t mChgFastChargeIndicatorPin = PIN_CHG_FAST;
        uint32_t mChrCurrentSettingsPin = PIN_CHG_ISET;

        hal::Battery BatteryHalInstance;
    };
}

#endif // SVC_PMC_MANAGER_H