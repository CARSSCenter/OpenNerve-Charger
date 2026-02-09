/**
 * @name Hornet / WPT Charger
 * @file eda_manager.cpp
 * @brief Manager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "eda_manager.h"
#include "../../../hal_layer/hal_gpio.h"

extern "C"
{
    void vApplicationIdleHook(void);
    void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                       StackType_t **ppxIdleTaskStackBuffer,
                                       uint32_t *pulIdleTaskStackSize);
    void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                        StackType_t **ppxTimerTaskStackBuffer,
                                        uint32_t *pulTimerTaskStackSize);
}

void vApplicationIdleHook(void)
{
    eda::Manager::IdleHook();
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] = {0U};
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] = {0U};
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

namespace eda
{
    void Manager::Initialize()
    {
        static constexpr uint32_t MAX_PIN_NUMBER = 0;
        static constexpr uint32_t MAX_PORT_NUMBER = 0;

        ClockInit();
        LogInit();

        for (uint32_t pinNumber = 0; pinNumber <= MAX_PIN_NUMBER; pinNumber++)
        {
            for (uint32_t pinPort = 0; pinPort <= MAX_PORT_NUMBER; pinPort++)
            {
                if (!((pinPort == PORT1) && (pinNumber > PORT1_MAX_PIN)))
                {
                    hal::Gpio::gpio_config_t pin;
                    pin.pin_number = NRF_GPIO_PIN_MAP(pinPort, pinNumber);
                    pin.direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_INPUT;
                    pin.input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT;
                    pin.pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_NOPULL;
                    pin.drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1;
                    pin.sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE;
                    hal::Gpio::ConfigurePin(pin);
                };
            };
        }
    };

    void Manager::StartEventDrivenArchitecture()
    {
        vTaskStartScheduler();
        while (1)
        {
        };
    }

    void Manager::LogInit()
    {
        LOG_WARNING("Start system initialization \n");
        // TODO: Initialize debug shell
        #if LOG_ENABLED
        // TODO: Handle initialization errors
        // TODO: Provide timestamp to logs
        NRF_LOG_INIT(NULL);
        NRF_LOG_DEFAULT_BACKENDS_INIT();
        #endif
        LOG_INFO("System initialization done \n");
        LOG_INFO("System Starts EDA\n");
    }

    void Manager::IdleHook()
    {
        #if LOG_ENABLED
        static bool logs_remaining = true;
        while (logs_remaining)
        {
            logs_remaining = NRF_LOG_PROCESS();
        }
        #endif
    }

    void Manager::ClockInit()
    {
        nrf_drv_clock_init();
    }

    void Manager::Delay(uint16_t ticks)
    {
        vTaskDelay(ticks);
    }
}