/**
 * @name Hornet / WPT Charger
 * @file hal_spi.cpp
 * @brief SPI Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_spi.h"

#include "sdk_errors.h"

#include <string.h>

namespace hal
{
    Spi::ErrorCode_e Spi::Init()
    {
        ret_code_t errCode = nrf_drv_spi_init(&(this->mSpiInstance), &(this->mSpiDriverConfig), nullptr, nullptr);
        if (errCode != NRF_SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }
        return ErrorCode_e::SUCCESS;
    };

    Spi::ErrorCode_e Spi::UnInit()
    {
        nrf_drv_spi_uninit(&this->mSpiInstance);
        return ErrorCode_e::SUCCESS;
    };

    Spi::ErrorCode_e Spi::Config(Config_t config, Instance_e instance)
    {
        switch (instance)
        {
        case Instance_e::INSTANCE_0:
            this->mSpiInstance = NRF_DRV_SPI_INSTANCE(0);
            break;
        case Instance_e::INSTANCE_1:
            this->mSpiInstance = NRF_DRV_SPI_INSTANCE(1);
            break;
        default:
            break;
        }
        ErrorCode_e errCode = Spi::ErrorCode_e::SUCCESS;

        this->mConfig.bitOrder = config.bitOrder;
        this->mConfig.clkFrequency = config.clkFrequency;
        this->mConfig.csPin = config.csPin;
        this->mConfig.mode = config.mode;
        this->mConfig.mosiPin = config.mosiPin;
        this->mConfig.sckPin = config.sckPin;
        this->mConfig.misoPin = config.misoPin;

        this->mSpiDriverConfig.bit_order = (nrf_drv_spi_bit_order_t)mConfig.bitOrder;
        this->mSpiDriverConfig.frequency = (nrf_drv_spi_frequency_t)mConfig.clkFrequency;
        this->mSpiDriverConfig.mode = (nrf_drv_spi_mode_t)mConfig.mode;
        this->mSpiDriverConfig.mosi_pin = this->mConfig.mosiPin;
        this->mSpiDriverConfig.miso_pin = this->mConfig.misoPin;
        this->mSpiDriverConfig.sck_pin = this->mConfig.sckPin;
        this->mSpiDriverConfig.ss_pin = mConfig.csPin;

        this->mSpiDriverConfig.orc = 0xFF;
        this->mSpiDriverConfig.irq_priority = SPI_DEFAULT_CONFIG_IRQ_PRIORITY;

        if (errCode == Spi::ErrorCode_e::SUCCESS)
        {
            this->mConfigDone = 1;
        }
        return errCode;
    };

    Spi::ErrorCode_e Spi::Write(uint8_t *data, uint16_t length)
    {
        memcpy(this->mTxBuffer.buffer, data, length);

        uint16_t tx_length = length;
        uint16_t rx_length = length;
        
        ret_code_t errCode = nrf_drv_spi_transfer(&(this->mSpiInstance), this->mTxBuffer.buffer, tx_length, this->mRxBuffer.buffer, rx_length);
        if (errCode != NRF_SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }

        return ErrorCode_e::SUCCESS;
    };

    Spi::ErrorCode_e Spi::Read(uint8_t *data, uint16_t length, void* data_r)
    {
        memcpy(this->mTxBuffer.buffer, data, length);
        
        uint16_t tx_length = length;
        uint16_t rx_length = length;
        

        ret_code_t errCode = nrf_drv_spi_transfer(&(this->mSpiInstance), this->mTxBuffer.buffer, tx_length, this->mRxBuffer.buffer, rx_length);
        
        if (errCode != NRF_SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }

        memcpy(data_r, this->mRxBuffer.buffer, length);

        return ErrorCode_e::SUCCESS;
    };

};
