/**
 * @name Hornet / WPT Charger
 * @file hal_ringbuffer.cpp
 * @brief Ring Buffer HAL implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_ringbuffer.h"

#include "hal_uart.h"

#include "nrf_ringbuf.h"
#include "sdk_errors.h"

#include <cstdint>

namespace hal
{

    template <typename Item>
    RingBuffer<Item>::RingBuffer()
    {
        nrf_ringbuf_init(&mRingBuffer);
    }

    template <typename Item>
    typename RingBuffer<Item>::ErrorCode_e RingBuffer<Item>::Get(Item &item)
    {
        if (IsEmpty())
        {
            return ErrorCode_e::EMPTY;
        }

        auto bytes = reinterpret_cast<uint8_t *>(&item);
        constexpr size_t EXPECTED_NUMBER_BYTES_OUT = sizeof(Item);

        size_t lengthOut = EXPECTED_NUMBER_BYTES_OUT;

        ret_code_t errCode = nrf_ringbuf_cpy_get(&mRingBuffer, bytes, &lengthOut);
        if (errCode != NRF_SUCCESS || lengthOut != EXPECTED_NUMBER_BYTES_OUT)
        {
            return ErrorCode_e::FAIL;
        }

        --mItemCount;

        return ErrorCode_e::SUCCESS;
    }

    template <typename Item>
    typename RingBuffer<Item>::ErrorCode_e RingBuffer<Item>::Put(const Item &item)
    {
        if (IsFull())
        {
            return ErrorCode_e::FULL;
        }

        const auto bytes = reinterpret_cast<const uint8_t *>(&item);
        constexpr size_t EXPECTED_NUMBER_BYTES_IN = sizeof(Item);

        size_t lengthIn = EXPECTED_NUMBER_BYTES_IN;

        ret_code_t errCode = nrf_ringbuf_cpy_put(&mRingBuffer, bytes, &lengthIn);
        if (errCode != NRF_SUCCESS || lengthIn != EXPECTED_NUMBER_BYTES_IN)
        {
            return ErrorCode_e::FAIL;
        }

        ++mItemCount;

        return ErrorCode_e::SUCCESS;
    }

    template <typename Item>
    bool RingBuffer<Item>::IsEmpty() const
    {
        return mItemCount == 0;
    }

    template <typename Item>
    bool RingBuffer<Item>::IsFull() const
    {
        return mItemCount >= MAX_NUMBER_ITEMS;
    }

    template <typename Item>
    size_t RingBuffer<Item>::GetItemCount() const
    {
        return mItemCount;
    }

    template class RingBuffer<Uart::Packet_t>;

}
