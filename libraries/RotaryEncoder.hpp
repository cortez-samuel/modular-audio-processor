#ifndef ROTARYENCODER_HPP
#define ROTARYENCODER_HPP

#include "pico/stdlib.h"

template<uint64_t BOUNCING_TIME>
class RotaryEncoder {
    static const uint64_t BOUNCING_TIME_us = BOUNCING_TIME;

    static inline RotaryEncoder* instances[32];


    uint _pinA, _pinB;

    uint64_t t0;
    bool bouncing;
    int position;       // state of rotary encoder

public:
    RotaryEncoder() {
        t0 = 0;
        bouncing = false;
        position = 0;        
    }
    RotaryEncoder(uint pinA, uint pinB) {
        t0 = 0;
        bouncing = false;
        position = 0;

        begin(pinA, pinB);
    }

public:
    void begin(uint pinA, uint pinB) {
        _pinA = pinA;
        _pinB = pinB;

        gpio_init(pinA);
        gpio_init(pinB);

        gpio_set_dir(pinA, 0);
        gpio_set_dir(pinB, 0);

        gpio_set_irq_enabled_with_callback(pinA, GPIO_IRQ_EDGE_RISE, true, _clsGPIOIRQ);

        instances[pinA] = this;
        instances[pinB] = this;
    }

public:
    inline bool isBouncing() {
        bouncing = (time_us_64() - t0) < BOUNCING_TIME_us;
        return bouncing;
    }
    inline int getPosition() const {
        return position;
    }
    inline void setPosition(int newPosition) {
        position = newPosition;
    }

private:
    inline void onRotate() {
        if (gpio_get(_pinB)) {
            position++;
        }
        else {
            position--;
        }
    }
    inline void onRotate_bouncing() {
        uint64_t now = time_us_64();

        if (!isBouncing()) {
            onRotate();
            t0 = now;
            bouncing = true;
        }
    }

private:
    void __time_critical_func(_GPIOIRQ)(uint gpio, uint32_t event_mask) {
        onRotate_bouncing();

        gpio_acknowledge_irq(gpio, event_mask);
    }
    static void __time_critical_func(_clsGPIOIRQ)(uint gpio, uint32_t event_mask) {
        RotaryEncoder* instance = instances[gpio];
        instance->_GPIOIRQ(gpio, event_mask);
    }
};


#endif