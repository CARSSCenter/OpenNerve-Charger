/**
 * @name Hornet / WPT Charger
 * @file hal_spi.h
 * @brief SPI Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "hal_pinout.h"

#include "nrf_drv_spi.h"

#include <cstdint>

namespace hal
{

    static constexpr uint16_t MAX_SPI_BUFFER_SIZE = 256;

    typedef struct
    {
        uint8_t buffer[MAX_SPI_BUFFER_SIZE];
        uint16_t length;
    } spi_buffer_t;

    typedef void (*HalSpiCallback_t)(void *p_context, spi_buffer_t *p_buffer);

    class Spi
    {
    public:
        enum class ErrorCode_e : uint8_t
        {
            SUCCESS,
            FAIL,
        };

        enum class ClockFrequency_e
        {
            CLOCK_FREQ_125K = NRF_DRV_SPI_FREQ_125K,
            CLOCK_FREQ_250K = NRF_DRV_SPI_FREQ_250K,
            CLOCK_FREQ_500K = NRF_DRV_SPI_FREQ_500K,
            CLOCK_FREQ_1M = NRF_DRV_SPI_FREQ_1M,
            CLOCK_FREQ_2M = NRF_DRV_SPI_FREQ_2M,
            CLOCK_FREQ_4M = NRF_DRV_SPI_FREQ_4M,
            CLOCK_FREQ_8M = NRF_DRV_SPI_FREQ_8M,
        };

        enum class Mode_e
        {
            MODE_0 = NRF_DRV_SPI_MODE_0,
            MODE_1 = NRF_DRV_SPI_MODE_1,
            MODE_2 = NRF_DRV_SPI_MODE_2,
            MODE_3 = NRF_DRV_SPI_MODE_3,
        };

        enum class BitOrder_e
        {
            MSB = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,
            LSB = NRF_DRV_SPI_BIT_ORDER_LSB_FIRST
        };

        enum class Instance_e
        {
            INSTANCE_0,
            INSTANCE_1,
        };

        struct Config_t
        {
            ClockFrequency_e clkFrequency;
            Mode_e mode;
            uint8_t csPin;
            BitOrder_e bitOrder;
            uint32_t sckPin;
            uint32_t mosiPin;
            uint32_t misoPin;
        };

        ErrorCode_e Init();

        ErrorCode_e UnInit();

        ErrorCode_e Config(Config_t config, Instance_e instance);

        ErrorCode_e Write(uint8_t *data, uint16_t length);

        ErrorCode_e Read(uint8_t *data, uint16_t length, void* data_r);

        Config_t mConfig;

    private:
        
        nrf_drv_spi_t mSpiInstance;

        nrf_drv_spi_config_t mSpiDriverConfig;

        spi_buffer_t mRxBuffer;

        spi_buffer_t mTxBuffer;

        uint8_t mConfigDone = 1;
    };
};

#endif