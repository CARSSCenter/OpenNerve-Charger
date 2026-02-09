/**
 * @name Hornet / WPT Charger
 * @file hal_dac_spi.h
 * @brief DAC SPI Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_DAC_SPI_H
#define HAL_DAC_SPI_H

#include "hal_spi.h"
#include "hal_pinout.h"
#include <cstddef>
#include <cstdint>

namespace hal
{

    /// DAC80504 SPI communication implementation.
    class Dac80504Spi
    {
    public:
        /// Registers accessible via SPI.
        enum class RegisterName : uint8_t
        {
            NOOP,
            DEVICE_ID,
            SYNC,
            CONFIG,
            GAIN,
            TRIGGER,
            BRDCAST,
            DAC0,
            DAC1,
            DAC2,
            DAC3,
            NUMBER_OF_ELEMENTS
        };

        enum class Operation : uint8_t
        {
            WRITE,
            READ
        };

        Dac80504Spi();

        /// Write data to the specified register.
        /// @param name name of the register to write.
        /// @param value Data to write.
        void WriteRegister(RegisterName name, uint16_t value);

        /// Read data from the specified register.
        /// @param name name of the register to read.
        /// @return Data read from the register.
        uint16_t ReadRegister(RegisterName name);

    private:
        struct Command_t
        {
            static constexpr size_t SIZE = 3;

            Command_t(uint8_t opcode, uint16_t data, Operation operation);

            uint8_t opcode;
            struct
            {
                uint8_t high;
                uint8_t low;
            } data;

            void ToBytes(uint8_t bytes[]) const;
        };

        static constexpr auto NUMBER_OF_COMMANDS = static_cast<size_t>(RegisterName::NUMBER_OF_ELEMENTS);
        const uint8_t mOpcodes[NUMBER_OF_COMMANDS] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0B};

        Spi mSpi;
    };

}

#endif
