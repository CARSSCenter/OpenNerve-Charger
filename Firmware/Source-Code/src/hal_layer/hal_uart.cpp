/**
 * @name Hornet / WPT Charger
 * @file hal_uart.cpp
 * @brief UART HAL implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_uart.h"

#include "nrf_drv_uart.h"
#include "sdk_errors.h"

namespace hal
{

    Uart::Uart(Config_t config) : mConfig(config)
    {
    }

    Uart::ErrorCode_e Uart::Init(HalRxEventHandler_t eventHandler, void *context)
    {
        ret_code_t errCode;
        if ((eventHandler == NULL) || (context == NULL))
        {
            return ErrorCode_e::FAIL;
        }
        mHalRxEventHandlerContext = context;
        mHalRxEventHandler = eventHandler;
        errCode = InitDriver();
        if (errCode != NRF_SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }

        errCode = StartReceiving();
        if (errCode != NRF_SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }

        return ErrorCode_e::SUCCESS;
    }

    void Uart::UnInit() const
    {
        nrf_drv_uart_uninit(&mDriverInstance);
    }

    Uart::ErrorCode_e Uart::Read(Packet_t &packet)
    {
        if (mRxPacketBuffer.IsEmpty())
        {
            return ErrorCode_e::EMPTY_BUFFER;
        }

        PacketBuffer::ErrorCode_e errCode = mRxPacketBuffer.Get(packet);
        if (errCode != PacketBuffer::ErrorCode_e::SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }
        return ErrorCode_e::SUCCESS;
    }

    Uart::ErrorCode_e Uart::Write(const Packet_t &packet)
    {
        if (packet.length == 0)
        {
            return ErrorCode_e::EMPTY_PACKET;
        }

        if (mTxPacketBuffer.IsFull())
        {
            return ErrorCode_e::FULL_BUFFER;
        }

        PacketBuffer::ErrorCode_e errCode = mTxPacketBuffer.Put(packet);
        if (errCode != PacketBuffer::ErrorCode_e::SUCCESS)
        {
            return ErrorCode_e::FAIL;
        }

        StartTransmitting();

        return ErrorCode_e::SUCCESS;
    }

    void Uart::DriverCallback(nrf_drv_uart_event_t *event, void *context)
    {
        auto instance = static_cast<Uart *>(context);
        instance->HandleDriverEvent[event->type](event, instance);
    }

    ret_code_t Uart::InitDriver()
    {
        nrf_drv_uart_config_t config = NRF_DRV_UART_DEFAULT_CONFIG;
        config.p_context = static_cast<void *>(this);
        config.baudrate = static_cast<nrf_uart_baudrate_t>(mConfig.baudrate);
        config.pselrxd = mConfig.rxPin;
        config.pseltxd = mConfig.txPin;

        ret_code_t errCode = nrf_drv_uart_init(&mDriverInstance, &config, DriverCallback);
#if LOG_ENABLED
        if ((Uart::ErrorCode_e)errCode == Uart::ErrorCode_e::SUCCESS)
        {
            LOG_INFO("HAL UART: UART peripheral initialized \n");
        }
        else
        {
            LOG_INFO("HAL UART: UART peripheral initialized failed \n");
        }
#endif
        return errCode;
    }

    ret_code_t Uart::StartReceiving()
    {
        ret_code_t errCode = nrf_drv_uart_rx(&mDriverInstance, mRxPacket.data, 1);
        return errCode;
    }

    void Uart::StartTransmitting()
    {
        if (nrf_drv_uart_tx_in_progress(&mDriverInstance))
        {
            return;
        }
        CallDriverCallbackWithTxEvent();
    }

    void Uart::CallDriverCallbackWithTxEvent()
    {
        nrf_drv_uart_event_t event = {.type = NRF_DRV_UART_EVT_TX_DONE};
        DriverCallback(&event, static_cast<void *>(this));
    }

    void Uart::HandleDriverTxEvent(nrf_drv_uart_event_t *event, Uart *instance)
    {
        PacketBuffer::ErrorCode_e errCode = instance->mTxPacketBuffer.Get(instance->mTxPacket);
        if (errCode != PacketBuffer::ErrorCode_e::SUCCESS)
        {
            return;
        }
        nrf_drv_uart_tx(&instance->mDriverInstance, instance->mTxPacket.data, instance->mTxPacket.length);
    }

    void Uart::HandleDriverRxEvent(nrf_drv_uart_event_t *event, Uart *instance)
    {
        // Received bytes counter has to be checked, since there could be an
        // event from RXTO interrupt
        if (event->data.rxtx.bytes > 0)
        {
            if (*event->data.rxtx.p_data == '\n')
            {
                PacketBuffer::ErrorCode_e errCode = instance->mRxPacketBuffer.Put(instance->mRxPacket);
                LOG_INFO("HAL UART: ABP Serial Message received \n");
                if (errCode != PacketBuffer::ErrorCode_e::SUCCESS)
                {
                    // Handle error when it is not possible to write a packet
                    // to the Rx buffer. As of now, the packet is discarded.
                }
                // execute event handler passed down on construction
                instance->mRxPacket.length = 0;
                instance->mHalRxEventHandler(instance->mHalRxEventHandlerContext);
            }
            else
            {
                // If a buffer overflow were to happen, discard the buffer
                // content and start overriding it from the start
                instance->mRxPacket.length = ++instance->mRxPacket.length % MAX_PACKET_LENGTH;

                // TODO: Set a corrupt message flag or handle this situation
                // in another way.
            }
        }
        nrf_drv_uart_rx(&instance->mDriverInstance, &instance->mRxPacket.data[instance->mRxPacket.length], 1);
    }

    void Uart::HandleDriverErrorEvent(nrf_drv_uart_event_t *event, Uart *instance) {}

}
