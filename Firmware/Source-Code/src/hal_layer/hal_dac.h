/**
 * @name Hornet / WPT Charger
 * @file hal_dac.h
 * @brief DAC Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_DAC_H
#define HAL_DAC_H

#include "hal_dac_spi.h"

#include <cstddef>
#include <cstdint>

namespace hal
{

    enum class Channel : uint8_t
    {
        CHANNEL_0,
        CHANNEL_1,
        CHANNEL_2,
        CHANNEL_3,
        NUMBER_OF_ELEMENTS,
    };

    /// External DAC interfacing class.
    class Dac
    {
    public:
        /// Initialize the DAC module.
        virtual void Init() = 0;

        /// Set the output voltage, in millivolts.
        virtual void SetOutput(Channel channel, uint16_t voltage) = 0;
    };

    /// DAC80504 DAC register representation.
    class Dac80504DacRegister
    {
    public:
        /// Register content after a reset.
        static constexpr uint16_t DEFAULT_VALUE = 0x0000;

        /// Read the register content.
        /// @return Register content.
        uint16_t GetContent(Channel channel);

        /// Write the register content.
        /// @param value Register content to write.
        void SetContent(Channel channel, uint16_t value);

    private:
        uint16_t mContent = DEFAULT_VALUE;
    };

    /// Texas Instruments DAC80504 DAC interfacing class.
    class Dac80504 : public Dac
    {
    public:
        /// Maximum output voltage in millivolts.
        static constexpr uint16_t MAX_OUTPUT_VOLTAGE = 2500;

        static constexpr uint16_t RESET_WORD = 0x000A;

        /// DAC reference divider values.
        enum class ReferenceDivider : uint16_t
        {
            DIVIDER_OFF = 0x0000, ///< Reference is not divided.
            DIVIDER_ON = 0x0100,  ///< Reference is divided by two.
        };

        /// DAC output gain values.
        enum class Gain : uint16_t
        {
            UNITARY_GAIN = 0x0000 , ///< Output gain is set to one.
            DOUBLE_GAIN = 0x000F,  ///< Output gain is set to two.
        };

        /// DAC broadcast values.
        enum class Brdcast : uint16_t
        {
            BRDCAST_EN = 0xFF00,
            BRDCAST_DISABLE = 0xF000,
        };

        /// DAC sync values.
        enum class Sync : uint16_t
        {
            SYNC_EN = 0xF00F,
            SYNC_DISABLE = 0xF000,
        };

        Dac80504();

        /// Initialize the DAC.
        void Init() override;

        /// Set the output voltage in millivolts.
        /// @param channel Channel to set.
        /// @param voltage Output voltage in millivolts.
        void SetOutput(Channel channel, uint16_t voltage) override;

        /// Read the output buffer gain field.
        /// @return Field content.
        uint8_t GetGain();

        /// Read the reference divider field.
        /// @return Field content.
        uint8_t GetReferenceDivider();
        
        /// Get the ID of the device.
        /// @return Device ID.
        uint16_t GetID();

    private:
        
        enum class GainRegisterFields : uint8_t
        {
            BUFF_GAIN,
            REF_DIV,
            NUMBER_OF_ELEMENTS,
        };
        
        static constexpr uint16_t MAX_DAC_DATA = 65535;

        static constexpr uint16_t REFERENCE_VOLTAGE = 2500;

        void SetSyncRegister(Brdcast brdcast, Sync sync);

        void SetGainRegister(ReferenceDivider referenceDivider, Gain gain);

        void SetDacData(Channel channel, uint16_t value);

        void SetTriggerRegister();
        
        void SetConfigRegister();

        uint16_t ComputeDacDataFromVoltage(uint16_t voltage);

        Dac80504Spi::RegisterName GetChannelRegister(Channel channel); 
        
        const uint16_t mGainRegisterFieldMasks[2] = {0x000F, 0x0100};

        Dac80504Spi mDacSpi;
    };

}

#endif
