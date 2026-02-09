/**
 * @name Hornet / WPT Charger
 * @file hal_dac_spi.cpp
 * @brief DAC SPI Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_dac_spi.h"

#include "../core_layer/event_driven_architecture/manager/eda_manager_log_config.h"

#include "hal_gpio.h"
#include "hal_spi.h"
#include "hal_pinout.h"

#include <cstddef>

namespace hal
{

    Dac80504Spi::Dac80504Spi()
    {
        Spi::Config_t config = {.clkFrequency = Spi::ClockFrequency_e::CLOCK_FREQ_125K,
                                .mode = Spi::Mode_e::MODE_1,
                                .csPin = PIN_DAC_CS,
                                .bitOrder = Spi::BitOrder_e::MSB,
                                .sckPin = PIN_SCLK,
                                .mosiPin = PIN_MOSI,
                                .misoPin = PIN_MISO};
        mSpi.Config(config, Spi::Instance_e::INSTANCE_0);
        mSpi.Init();
    }

    void Dac80504Spi::WriteRegister(RegisterName name, uint16_t value)
    {
        const uint8_t opcode = mOpcodes[static_cast<size_t>(name)];
        const Command_t command(opcode, value, Operation::WRITE);

        constexpr size_t NUMBER_OF_BYTES_TO_SEND = Command_t::SIZE;

        uint8_t bytesToSend[NUMBER_OF_BYTES_TO_SEND];
        command.ToBytes(bytesToSend);
        
        hal::Gpio::Write(PIN_DAC_CS, 0);
        Spi::ErrorCode_e errCode = mSpi.Write(bytesToSend, NUMBER_OF_BYTES_TO_SEND);
        hal::Gpio::Write(PIN_DAC_CS, 1);

        if (errCode != Spi::ErrorCode_e::SUCCESS)
        {
            // TODO: Handle errors appropriately.
            LOG_ERROR("Error writing to DAC register");
        }
    }

    uint16_t Dac80504Spi::ReadRegister(RegisterName name)
    {
        const uint8_t opcode = mOpcodes[static_cast<size_t>(name)];
        const Command_t command(opcode, 0, Operation::READ);

        constexpr size_t NUMBER_OF_BYTES_TO_SEND = Command_t::SIZE;
        constexpr size_t NUMBER_OF_BYTES_TO_RECEIVE = Command_t::SIZE;

        uint8_t bytesToSend[NUMBER_OF_BYTES_TO_SEND];
        command.ToBytes(bytesToSend);
        
        //This delay is necessary to mitigate an issue with the devkit SPI CS that
        //goes high before the last bit is sent. This occurs randonmly and with these delays
        //it can be avoided most of the times.
        for(uint32_t i = 0; i < 3000000; i++)
        {
        }

        hal::Gpio::Write(PIN_DAC_CS, 0);
        Spi::ErrorCode_e errCode = mSpi.Write(bytesToSend, NUMBER_OF_BYTES_TO_SEND);
        hal::Gpio::Write(PIN_DAC_CS, 1);
        if (errCode != Spi::ErrorCode_e::SUCCESS)
        {
            // TODO: Handle errors appropriately.
            LOG_ERROR("Error writing to DAC register");
        }

        uint8_t bytesReceived[NUMBER_OF_BYTES_TO_RECEIVE];
        const Command_t command_r(0xFF, 0xFFFF, Operation::READ);
        command_r.ToBytes(bytesToSend);
        
        //This delay is necessary to mitigate an issue with the devkit SPI CS that
        //goes high before the last bit is sent. This occurs randonmly and with these delays
        //it can be avoided most of the times.
        for(uint32_t i = 0; i < 3000000; i++)
        {
        }
        
        hal::Gpio::Write(PIN_DAC_CS, 0);
        errCode = mSpi.Read(bytesToSend,NUMBER_OF_BYTES_TO_RECEIVE, bytesReceived);
        hal::Gpio::Write(PIN_DAC_CS, 1);

        if (errCode != Spi::ErrorCode_e::SUCCESS)
        {
            LOG_ERROR("Error reading from DAC register");
        }

        uint16_t value = (static_cast<uint16_t>(bytesReceived[1]) << 8) | static_cast<uint16_t>(bytesReceived[2]);
        
        return value;
    }

    Dac80504Spi::Command_t::Command_t(uint8_t opcode, uint16_t data, Operation operation)
    {
        if (operation == Operation::WRITE)
        {
            this->opcode = opcode;
        }
        else if (operation == Operation::READ)
        {
            this->opcode = opcode | 0x80;
        }
        this->data.high = (data >> 8) & 0x00FF;
        this->data.low = data & 0x00FF;
    }

    void Dac80504Spi::Command_t::ToBytes(uint8_t bytes[]) const
    {
        bytes[0] = opcode;
        bytes[1] = data.high;
        bytes[2] = data.low;
    }

}
