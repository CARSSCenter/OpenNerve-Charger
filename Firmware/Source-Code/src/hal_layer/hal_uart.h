/**
 * @name Hornet / WPT Charger
 * @file hal_uart.h
 * @brief UART HAL declaration
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "hal_ringbuffer.h"
#include "../../core_layer/event_driven_architecture/manager/eda_manager.h"
#include "nrf_drv_uart.h"
#include "sdk_errors.h"

#include <cstdint>

namespace hal
{

    typedef void (*HalRxEventHandler_t)(void *);

    /// UART class which abstracts away the hardware.
    class Uart
    {
    public:
        /// Maximum length of a packet.
        static constexpr uint32_t MAX_PACKET_LENGTH = 250;

        /// Number of possible events for which a callback is needed. ToDo: If greater than 1, a list of handlers needs to be implemented
        static constexpr size_t NUMBER_HAL_EVENTS = 1;

        /// Packet which holds a UART message.
        struct Packet_t
        {
            uint8_t data[MAX_PACKET_LENGTH]; ///< Packet payload.
            uint32_t length = 0;             ///< Size, in bytes, of the payload.
        };

        /// UART baudrate.
        enum class Baudrate_e : uint32_t
        {
            BAUD_1200 = NRF_UART_BAUDRATE_1200,
            BAUD_2400 = NRF_UART_BAUDRATE_2400,
            BAUD_4800 = NRF_UART_BAUDRATE_4800,
            BAUD_9600 = NRF_UART_BAUDRATE_9600,
            BAUD_14400 = NRF_UART_BAUDRATE_14400,
            BAUD_19200 = NRF_UART_BAUDRATE_19200,
            BAUD_28800 = NRF_UART_BAUDRATE_28800,
            BAUD_38400 = NRF_UART_BAUDRATE_38400,
            BAUD_57600 = NRF_UART_BAUDRATE_57600,
            BAUD_76800 = NRF_UART_BAUDRATE_76800,
            BAUD_115200 = NRF_UART_BAUDRATE_115200,
            BAUD_230400 = NRF_UART_BAUDRATE_230400,
            BAUD_250000 = NRF_UART_BAUDRATE_250000,
            BAUD_460800 = NRF_UART_BAUDRATE_460800,
            BAUD_921600 = NRF_UART_BAUDRATE_921600,
            BAUD_1000000 = NRF_UART_BAUDRATE_1000000
        };

        /// UART peripheral configuration.
        struct Config_t
        {
            Baudrate_e baudrate;
            uint32_t rxPin;
            uint32_t txPin;
        };

        /// UART error code data type.
        enum class ErrorCode_e : uint8_t
        {
            SUCCESS,      ///< Operation completed without errors.
            FAIL,         ///< Unrecoverable error.
            EMPTY_BUFFER, ///< Empty buffer when trying to read a packet.
            FULL_BUFFER,  ///< Full buffer when trying to write a packet.
            EMPTY_PACKET, ///< Tried to send an empty packet.
        };

        Uart(Config_t config);

        /// Initialize the UART peripheral.
        ErrorCode_e Init(HalRxEventHandler_t eventHandler, void *context);

        /// Deinitialize the UART peripheral.
        void UnInit() const;

        /// Read the last UART packet from the reception buffer.
        /// @param packet Retrieved packet.
        /// @return `SUCCESS` if the operation is successfull.
        /// @return `FAIL` if the buffer is empty.
        ErrorCode_e Read(Packet_t &packet);

        /// Write a UART packet to the transmission buffer.
        /// @param packet Packet to be written.
        /// @return `SUCCESS` if the operation is successfull.
        /// @return `FAIL` if the buffer is full.
        ErrorCode_e Write(const Packet_t &packet);

        HalRxEventHandler_t mHalRxEventHandler;
        void *mHalRxEventHandlerContext;

    private:
        using PacketBuffer = RingBuffer<Packet_t>;

        using DriverEventHandler = void(nrf_drv_uart_event_t *event, Uart *instance);
        static constexpr size_t NUMBER_DRIVER_EVENTS = 3;

        static void DriverCallback(nrf_drv_uart_event_t *event, void *context);

        ret_code_t InitDriver();
        ret_code_t StartReceiving();
        void StartTransmitting();

        void CallDriverCallbackWithTxEvent();

        static void HandleDriverTxEvent(nrf_drv_uart_event_t *event, Uart *instance);
        static void HandleDriverRxEvent(nrf_drv_uart_event_t *event, Uart *instance);
        static void HandleDriverErrorEvent(nrf_drv_uart_event_t *event, Uart *instance);

        DriverEventHandler *HandleDriverEvent[NUMBER_DRIVER_EVENTS] =
            {
                HandleDriverTxEvent,
                HandleDriverRxEvent,
                HandleDriverErrorEvent};

        Packet_t mRxPacket;
        PacketBuffer mRxPacketBuffer;
        Packet_t mTxPacket;
        PacketBuffer mTxPacketBuffer;
        const Config_t mConfig;
        const nrf_drv_uart_t mDriverInstance = NRF_DRV_UART_INSTANCE(0);
    };

}

#endif
