/**
 * @name Hornet / WPT Charger
 * @file hal_gpio.h
 * @brief GPIO Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#define PORT0 0
#define PORT1 1
#define PORT1_MAX_PIN P1_PIN_NUM - 1

#include "nrf_drv_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

#include <cstdint>

namespace hal
{
    typedef void (*HalPinEventHandler_t)();

    /**
     * @brief Struct that wrap an EventHandler for a specific GPIO pin.
     */
    struct gpio_call_back_t
    {
        HalPinEventHandler_t event_handler; /**< Event handler for the interruption */
        nrfx_gpiote_pin_t pin_num;          /**< Pin number */
        nrf_gpiote_polarity_t polarity;     /**< Polarity for the GPIOTE channel */
    };

    /**
     * @brief GPIO Hardware Abstraction Layer class
     */
    class Gpio
    {
    public:
        /**
         * @brief alias type for nrf_gpio_pin_dir_t
         */
        enum class gpio_pin_dir_t : uint32_t
        {
            NRF_GPIO_PIN_DIR_INPUT = GPIO_PIN_CNF_DIR_Input,
            NRF_GPIO_PIN_DIR_OUTPUT = GPIO_PIN_CNF_DIR_Output
        };

        /**
         * @brief alias type for nrf_gpio_pin_input_t
         */
        enum class gpio_pin_input_t : uint32_t
        {
            NRF_GPIO_PIN_INPUT_CONNECT = GPIO_PIN_CNF_INPUT_Connect,
            NRF_GPIO_PIN_INPUT_DISCONNECT = GPIO_PIN_CNF_INPUT_Disconnect
        };

        /**
         * @brief alias type for nrf_gpio_pin_pull_t
         */
        enum class gpio_pin_pull_t : uint32_t
        {
            NRF_GPIO_PIN_NOPULL = GPIO_PIN_CNF_PULL_Disabled,
            NRF_GPIO_PIN_PULLDOWN = GPIO_PIN_CNF_PULL_Pulldown,
            NRF_GPIO_PIN_PULLUP = GPIO_PIN_CNF_PULL_Pullup
        };

        /**
         * @brief alias type for nrf_gpio_pin_drive_t
         */
        enum class gpio_pin_drive_t : uint32_t
        {
            NRF_GPIO_PIN_S0S1 = GPIO_PIN_CNF_DRIVE_S0S1,
            NRF_GPIO_PIN_H0S1 = GPIO_PIN_CNF_DRIVE_H0S1,
            NRF_GPIO_PIN_S0H1 = GPIO_PIN_CNF_DRIVE_S0H1,
            NRF_GPIO_PIN_H0H1 = GPIO_PIN_CNF_DRIVE_H0H1,
            NRF_GPIO_PIN_D0S1 = GPIO_PIN_CNF_DRIVE_D0S1,
            NRF_GPIO_PIN_D0H1 = GPIO_PIN_CNF_DRIVE_D0H1,
            NRF_GPIO_PIN_S0D1 = GPIO_PIN_CNF_DRIVE_S0D1,
            NRF_GPIO_PIN_H0D1 = GPIO_PIN_CNF_DRIVE_H0D1
        };

        /**
         * @brief alias type for nrf_gpio_pin_sense_t
         */
        enum class gpio_pin_sense_t : uint32_t
        {
            NRF_GPIO_PIN_NOSENSE = GPIO_PIN_CNF_SENSE_Disabled,
            NRF_GPIO_PIN_SENSE_LOW = GPIO_PIN_CNF_SENSE_Low,
            NRF_GPIO_PIN_SENSE_HIGH = GPIO_PIN_CNF_SENSE_High
        };

        /**
         * @brief Struct that represents a GPIO pin configuration.
         */
        struct gpio_config_t
        {
            uint32_t pin_number;      /**< Pin number */
            gpio_pin_dir_t direction; /**< Pin direction (input or output) */
            gpio_pin_input_t input;   /**< Pin connected/disconnected to input buffer. */
            gpio_pin_pull_t pull;     /**< Pin pull-up/pull-down configuration */
            gpio_pin_drive_t
                drive;              /**< Pin drive configuration: used for selecting output drive mode. */
            gpio_pin_sense_t sense; /**< Pin sense high or low level */
        };

        /**
         * @brief Initializes a GPIOs pins
         */
        static void Init();

        /**
         * @brief Configure a GPIO using basic configuration. Use this function if
         *  intended to read/write without interruptions or events.
         * @param config Configuration of the pin.
         */
        static void ConfigurePin(const gpio_config_t config);

        /**
         * @brief Function configuring a GPIOTE output pin.
         * @param pin_num Pin number
         * @param config Configuration of the pin.
         */
        static void ConfigurePin(uint32_t pin_number, nrf_drv_gpiote_out_config_t config);

        /**
         * @brief Function configuring a GPIOTE input pin.
         * @param pin_num Pin number
         * @param polarity Polarity for the GPIOTE channel that triggers an interruption.
         * @param config Configuration of the pin.
         * @param handler Event handler for the pin
         */
        static void ConfigurePin(uint32_t pin_num,
                                 nrf_gpiote_polarity_t polarity,
                                 nrf_drv_gpiote_in_config_t config,
                                 HalPinEventHandler_t handler);

        /**
         * @brief Reads the state of a GPIO pin
         * @param pin_number Pin number to read
         * @return Pin state (0 or 1)
         */
        static uint8_t Read(uint32_t pin_number);

        /**
         * @brief Reads the state of the list of GPIOs given as parameters
         * @param pin_numbers List of pins numbers to read
         * @param num_pins Number of pins to read
         * @return List of all pin states where just the states of pin_numbers are updated
         */
        static uint8_t *ReadMultiple(const uint32_t *pin_numbers, uint32_t num_pins);

        /**
         * @brief Sets the state of a GPIO pin
         * @param pin_number Pin number to set
         * @param value Pin state to set (0 or 1)
         */
        static void Write(uint32_t pin_number, uint32_t value);

        /**
         * @brief Sets the state of a list of GPIOs pin
         * @param pin_number Lis of pin numbers to set
         * @param values Values of the pins to set (0 or 1)
         * @param num_pins Number of pins to write
         */
        static void WriteMultiple(const uint32_t *pin_numbers,
                                  const uint32_t *values,
                                  uint32_t num_pins);

    private:
        /**
         * @brief Helper function to save the event handler for a specific pin.
         * @param pin_num Pin number
         * @param polarity Polarity for the GPIOTE channel that triggers an interruption.
         * @param event_handler Event handler for the pin
         */
        static void SetCallback(HalPinEventHandler_t event_handler,
                                nrfx_gpiote_pin_t pin,
                                nrf_gpiote_polarity_t polarity);

        /**
         * @brief Helper function for configuring a pin
         * @param pin_num Pin number
         * @param polarity Polarity for the GPIOTE channel that triggers an interruption.
         */
        static nrfx_gpiote_evt_handler_t EventHandler(nrfx_gpiote_pin_t pin_num,
                                                      nrf_gpiote_polarity_t polarity);

        static gpio_call_back_t mCallBacks[40];
        static uint8_t mPinValues[40];
    };
}
#endif
