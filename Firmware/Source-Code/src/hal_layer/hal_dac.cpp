/**
 * @name Hornet / WPT Charger
 * @file hal_dac.cpp
 * @brief DAC Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_dac.h"

#include "hal_dac_spi.h"
#include "hal_spi.h"

#include "../core_layer/event_driven_architecture/manager/eda_manager.h"

#include <cstddef>
#include <cstdint>

namespace hal
{

    Dac80504::Dac80504() : mDacSpi()
    {
    }

    void Dac80504::Init()
    {
        SetTriggerRegister();

        SetSyncRegister(Brdcast::BRDCAST_DISABLE, Sync::SYNC_DISABLE);

        SetConfigRegister();

        SetGainRegister(ReferenceDivider::DIVIDER_OFF, Gain::DOUBLE_GAIN);
    }

    void Dac80504::SetOutput(Channel channel, uint16_t voltage)
    {
        uint16_t dacData = ComputeDacDataFromVoltage(voltage);
        SetDacData(channel, dacData);
    }

    void Dac80504::SetDacData(Channel channel, uint16_t value)
    {

        mDacSpi.WriteRegister(GetChannelRegister(channel), value);
    }

    uint16_t Dac80504::ComputeDacDataFromVoltage(uint16_t voltage)
    {
        // Operations are performed using 32-bit integers to prevent integer
        // overflow
        ReferenceDivider mreferenceDivider = ReferenceDivider::DIVIDER_OFF;
        Gain mgain = Gain::DOUBLE_GAIN;
        uint32_t referenceDivider = (mreferenceDivider == ReferenceDivider::DIVIDER_OFF) ? 1 : 2;
        uint32_t gain = (mgain == Gain::UNITARY_GAIN) ? 1 : 2;

        uint32_t maxDacData = MAX_DAC_DATA;
        uint32_t referenceVoltage = REFERENCE_VOLTAGE;

        // As per the Dac80504 datasheet (page 21, Section
        // 8.3.1.1: DAC Transfer Function).
        uint16_t dacData = static_cast<uint16_t>(
            voltage * (maxDacData + 1) * referenceDivider / (gain * referenceVoltage));

        return dacData;
    }

    void Dac80504::SetSyncRegister(Brdcast brdcast, Sync sync)
    {
        uint16_t content = static_cast<uint16_t>(brdcast) | static_cast<uint16_t>(sync);

        mDacSpi.WriteRegister(Dac80504Spi::RegisterName::SYNC, content);
    }

    void Dac80504::SetConfigRegister()
    {
        uint16_t content = 0x0000;

        mDacSpi.WriteRegister(Dac80504Spi::RegisterName::CONFIG, content);
    }

    void Dac80504::SetGainRegister(ReferenceDivider referenceDivider, Gain gain)
    {
        uint16_t content = static_cast<uint16_t>(referenceDivider) | static_cast<uint16_t>(gain);

        mDacSpi.WriteRegister(Dac80504Spi::RegisterName::GAIN, content);
    }

    void Dac80504::SetTriggerRegister()
    {
        mDacSpi.WriteRegister(Dac80504Spi::RegisterName::TRIGGER, Dac80504::RESET_WORD);
    }

    uint8_t Dac80504::GetReferenceDivider()
    {
        uint16_t gainRegister = mDacSpi.ReadRegister(Dac80504Spi::RegisterName::GAIN);

        auto referenceDividerBytes = (gainRegister & mGainRegisterFieldMasks[static_cast<size_t>(GainRegisterFields::REF_DIV)]) >> 8;

        return static_cast<uint8_t>(referenceDividerBytes);
    }

    uint8_t Dac80504::GetGain()
    {
        uint16_t gainRegister = mDacSpi.ReadRegister(Dac80504Spi::RegisterName::GAIN);

        auto gainBytes = gainRegister & mGainRegisterFieldMasks[static_cast<size_t>(GainRegisterFields::BUFF_GAIN)];

        return static_cast<uint8_t>(gainBytes);
    }

    uint16_t Dac80504::GetID()
    {
        uint16_t idRegister = mDacSpi.ReadRegister(Dac80504Spi::RegisterName::DEVICE_ID);
        return idRegister;
    }

    Dac80504Spi::RegisterName Dac80504::GetChannelRegister(Channel channel)
    {
        switch (channel)
        {
        case Channel::CHANNEL_0:
        {
            return Dac80504Spi::RegisterName::DAC0;
            break;
        }
        case Channel::CHANNEL_1:
        {
            return Dac80504Spi::RegisterName::DAC1;
            break;
        }
        case Channel::CHANNEL_2:
        {
            return Dac80504Spi::RegisterName::DAC2;
            break;
        }
        case Channel::CHANNEL_3:
        {
            return Dac80504Spi::RegisterName::DAC3;
            break;
        }
        default:
        {
            return Dac80504Spi::RegisterName::DAC0;
            break;
        }
        }
    }
}