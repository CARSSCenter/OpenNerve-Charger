/**
 * @name Hornet / WPT Charger
 * @file hal_gpio.h
 * @brief LED Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef HAL_LED_H
#define HAL_LED_H

#include "hal_gpio.h"
#include "hal_pinout.h"
#include "nrfx_pwm.h"
#include <cstdint>

#define LEDS_ARRAY_QTY 3
#define LEDS_SEQUENCE_LENGTH_PER_LED 24                                         /**< Each LED needs 24 bits (Amount of PWM values) */
#define LEDS_SEQUENCE_LENGTH_TOTAL LEDS_ARRAY_QTY *LEDS_SEQUENCE_LENGTH_PER_LED /**< Total of bits for all LEDs */

/**<  Each bit is codified as a PWM signal of fixed period */
/**<  Each tick lasts 1/16 MHz = 0.0625 us */
#define LEDS_PERIOD_TICKS 20   /**< 20/16MHz = 1.25us (should be 1.25us +-0.15us) */
#define LEDS_ONE_HIGH_TICKS 13 /**< 13/16MHz = 0.81us (should be 0.80us +-0.15us) */
#define LEDS_ZERO_HIGH_TICKS 7 /**< 7/16MHz = 0.44us (should be 0.40us +-0.15us) */

#define LEDS_CODED_ONE ((uint16_t)(LEDS_ONE_HIGH_TICKS | 0x8000))   /**< PWM Duty cycle for coding a 1 */
#define LEDS_CODED_ZERO ((uint16_t)(LEDS_ZERO_HIGH_TICKS | 0x8000)) /**< PWM Duty cycle for coding a 0 */

namespace hal
{
    enum class LedColor_e : uint8_t
    {
        RED,
        GREEN,
        BLUE,
        YELLOW,
        CYAN,
        MAGENTA,
        WHITE
    };

    enum class LedState_e : uint8_t
    {
        OFF,
        ON
    };

    enum class LedPosition_e : uint8_t
    {
        LED1 = 0,
        LED2 = 1,
        LED3 = (LEDS_ARRAY_QTY - 1),
    };

    enum class LedIntensity_e : uint8_t
    {
        OFF = 0,
        LOW = 20,
        MEDIUM = 45,
        HIGH = 100,
    };

    typedef struct
    {
        LedColor_e color;

        LedIntensity_e intensity = LedIntensity_e::HIGH;

        LedState_e state = LedState_e::OFF;
    } RgbLed_t;

    class Leds
    {
    public:
        // Static methods for Singleton pattern
        static Leds& GetInstance();
        static void Initialize();
        static void Uninitialize();

        // Instance methods
        void TurnLedOn(RgbLed_t *led);
        void TurnLedOff(RgbLed_t *led);
        LedState_e GetLedState(RgbLed_t led);
        void SetLedColor(RgbLed_t *led, LedPosition_e position, LedColor_e color);
        void SetLedIntensity(RgbLed_t *led, LedIntensity_e intensity);

        // BLE Scanning
        void LedScanOn(bool enable);

        // Battery charging
        void LedCharging(bool enable);
        
        // Battery slow charging
        void LedChargingSlow(bool enable);

        // Battery charged
        void LedCharged(bool enable);

        // Test LEDs
        void TestLeds();

        // Public member
        RgbLed_t rgb_led;

    private:
        // Private constructor for Singleton pattern
        Leds();

        // Delete copy operations to prevent duplication
        Leds(const Leds&) = delete;
        Leds& operator=(const Leds&) = delete;

        void Init(void);
        void UnInit(void);
        void ConfigureLedColor(RgbLed_t *led, LedPosition_e position, uint8_t red, uint8_t green, uint8_t blue);
        void Show(void);
        void Clear(RgbLed_t *led);
        void ClearPosition(RgbLed_t *led, LedPosition_e position);

    protected:
        nrfx_pwm_t m_pwm_driver;
        static nrf_pwm_values_common_t s_sequence[LEDS_SEQUENCE_LENGTH_TOTAL];
        
        static Leds s_instance;
        static bool s_initialized;
    };
}

#endif