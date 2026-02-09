/**
 * @name Hornet / WPT Charger
 * @file hal_ringbuffer.h
 * @brief Ring Buffer HAL declaration
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_RINGBUFFER_H
#define HAL_RINGBUFFER_H

#include "nrf_ringbuf.h"

#include <cstdint>

namespace hal
{

    /// Ring buffer wrapper which provides a way to share data.
    template <typename Item>
    class RingBuffer
    {
    public:
        /// Maximum number of bytes that can be stored.
        ///
        /// It must be a power of two.
        static constexpr size_t BUFFER_LENGTH = 512;

        /// Maximum number of items that can be held by the buffer.
        static constexpr size_t MAX_NUMBER_ITEMS = BUFFER_LENGTH / sizeof(Item);

        /// Ring buffer error code data type.
        enum class ErrorCode_e : uint8_t
        {
            SUCCESS, ///< Operation completed without errors.
            FAIL,    ///< Unrecoverable error.
            EMPTY,   ///< Empty buffer when trying to read an item.
            FULL,    ///< Full buffer when trying to write an item.
        };

        RingBuffer();

        /// Read an item from the buffer.
        /// @param item Read item.
        /// @return `SUCCESS` if the operation was successful.
        /// @return `FAIL` if the operation failed.
        ErrorCode_e Get(Item &item);

        /// Write an item to the buffer.
        /// @param item Item to write into the buffer.
        /// @return `SUCCESS` if the operation was successful.
        /// @return `FAIL` if the operation failed.
        ErrorCode_e Put(const Item &item);

        /// Return whether there are any items in the buffer.
        /// @return `true` if the buffer is empty.
        /// @return `false` if there is at least one item in the buffer.
        bool IsEmpty() const;

        /// Return whether there is space left in the buffer.
        /// @return `true` if the buffer is full.
        /// @return `false` if there is space for at least one item in the buffer.
        bool IsFull() const;

        /// Return the number of items currently stored in the buffer.
        /// @return Number of items in the buffer.
        size_t GetItemCount() const;

    private:
        uint8_t mBuffer[BUFFER_LENGTH];
        nrf_ringbuf_cb_t mControlBlock;
        const nrf_ringbuf_t mRingBuffer =
            {
                .p_buffer = mBuffer,
                .bufsize_mask = BUFFER_LENGTH - 1,
                .p_cb = &mControlBlock};

        size_t mItemCount = 0;
    };

}

#endif
