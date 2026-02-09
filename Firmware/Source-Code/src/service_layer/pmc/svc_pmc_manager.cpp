/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_manager.cpp
 * @brief PmcManager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_pmc_manager.h"

#include "eda_manager_log_config.h"

#include "hal_gpio.h"

namespace svc
{
    PmcManager &PmcManager::Instance()
    {
        static PmcManager instance;
        return instance;
    }

    PmcManager::PmcManager() : BatteryHalInstance()
    {
    }

    void PmcManager::ConfigureGpios()
    {
        LOG_DEBUG("PMC Manager: ConfigGpios\n");
        hal::Gpio::gpio_config_t vccEnablePinConfig = {
            .pin_number = mVccEnablePin,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_OUTPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_NOPULL,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(vccEnablePinConfig);
        hal::Gpio::Write(mVccEnablePin, ENABLE_INACTIVE_LEVEL);

        hal::Gpio::gpio_config_t ChgEnablePinConfig = {
            .pin_number = mChgEnablePin,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_INPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_NOPULL,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(ChgEnablePinConfig);
        hal::Gpio::Write(mChgEnablePin, ENABLE_INACTIVE_LEVEL);

        nrf_drv_gpiote_in_config_t ChgPresentIndicatorPinConfig = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
        ChgPresentIndicatorPinConfig.pull = NRF_GPIO_PIN_PULLUP;

        hal::Gpio::ConfigurePin(mChrPwrPresentIndicatorPin,
                                NRF_GPIOTE_POLARITY_TOGGLE,
                                ChgPresentIndicatorPinConfig,
                                StaticReadBatteryChargerPresentIndicator);

        nrf_drv_gpiote_in_config_t ChgChargeIndicatorPinConfig = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
        ChgChargeIndicatorPinConfig.pull = NRF_GPIO_PIN_PULLUP;

        hal::Gpio::ConfigurePin(mChgChargeIndicatorPin,
                                NRF_GPIOTE_POLARITY_TOGGLE,
                                ChgChargeIndicatorPinConfig,
                                StaticReadBatteryChargeIndicator);

        nrf_drv_gpiote_in_config_t ChgFastChargeIndicatorPinConfig = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
        ChgFastChargeIndicatorPinConfig.pull = NRF_GPIO_PIN_PULLUP;

        hal::Gpio::ConfigurePin(mChgFastChargeIndicatorPin,
                                NRF_GPIOTE_POLARITY_TOGGLE,
                                ChgFastChargeIndicatorPinConfig,
                                StaticReadBatteryFastChargeIndicator);

        hal::Gpio::gpio_config_t mChrCurrentSettingsPinConfig = {
            .pin_number = mChrCurrentSettingsPin,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_INPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_PULLUP,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(mChrCurrentSettingsPinConfig);
    }

    void PmcManager::Init()
    {
        LOG_DEBUG("PMC Manager: Init\n");
        ConfigureGpios();
    }

    void PmcManager::EnableVccRegulator()
    {
        LOG_DEBUG("PMC Manager: EnableVccRegulator\n");
        hal::Gpio::Write(mVccEnablePin, ENABLE_ACTIVE_LEVEL);
    }

    void PmcManager::DisableVccRegulator()
    {
        LOG_DEBUG("PMC Manager: DisableVccRegulator\n");
        hal::Gpio::Write(mVccEnablePin, ENABLE_INACTIVE_LEVEL);
    }

    void PmcManager::EnableBatteryCharger()
    {
        LOG_DEBUG("PMC Manager: EnableBatteryCharger\n");
        hal::Gpio::Write(mChgEnablePin, ENABLE_ACTIVE_LEVEL);
    }

    void PmcManager::DisableBatteryCharger()
    {
        LOG_DEBUG("PMC Manager: DisableBatteryCharger\n");
        hal::Gpio::Write(mChgEnablePin, ENABLE_INACTIVE_LEVEL);
    }

    void PmcManager::GetBatteryVoltage()
    {
        BatteryHalInstance.GetBatteryVoltage();
        LOG_DEBUG("PMC Manager: GetBatteryVoltage\n");
    }

    void PmcManager::StaticReadBatteryChargerPresentIndicator()
    {
        Instance().ReadBatteryChargerPresentIndicator();
    }

    void PmcManager::ReadBatteryChargerPresentIndicator()
    {
        uint8_t mChrPrInd = 0;
        LOG_DEBUG("PMC Manager: ReadBatteryChargerPresentIndicator\n");
        mChrPrInd = hal::Gpio::Read(mChrPwrPresentIndicatorPin);

        if (mChrPrInd == READ_ACTIVE_LEVEL)
        {
          PmcPort::SendEvent(PmcPort::Event_e::PMC_READ_CHARGER_PRESENT_INDICATOR, NULL);
        }
    }

    void PmcManager::StaticReadBatteryChargeIndicator()
    {
        Instance().ReadBatteryChargeIndicator();
    }

    void PmcManager::ReadBatteryChargeIndicator()
    {
        uint8_t mChrInd = 0;
        LOG_DEBUG("PMC Manager: ReadBatteryChargeIndicatorPin\n");
        mChrInd = hal::Gpio::Read(mChgChargeIndicatorPin);

        if (mChrInd == READ_ACTIVE_LEVEL)
        {
           PmcPort::SendEvent(PmcPort::Event_e::PMC_BATTERY_CHARGED, NULL);
        }
        else 
        {
           PmcPort::SendEvent(PmcPort::Event_e::PMC_BATTERY_CHARGING, NULL);
        }
    }

    void PmcManager::StaticReadBatteryFastChargeIndicator()
    {
        Instance().ReadBatteryFastChargeIndicator();
    }

    void PmcManager::ReadBatteryFastChargeIndicator()
    {
        uint8_t mChrFastInd = 0;
        LOG_DEBUG("PMC Manager: ReadBatteryFastChargeIndicator\n");
        mChrFastInd = hal::Gpio::Read(mChgFastChargeIndicatorPin);

        if (mChrFastInd == READ_INACTIVE_LEVEL)
        {
           PmcPort::SendEvent(PmcPort::Event_e::PMC_READ_FAST_CHARGER_INDICATOR, NULL);
        }
    }

    void PmcManager::ReadBatteryCurrentSettings()
    {
        uint8_t mIset = 0;
        LOG_DEBUG("PMC Manager: ReadBatteryCurrentSettings\n");
        mIset = hal::Gpio::Read(mChrCurrentSettingsPin);

        PmcPort::SendEvent(PmcPort::Event_e::PMC_READ_ISET, reinterpret_cast<uint32_t>(&mIset));
    }
}