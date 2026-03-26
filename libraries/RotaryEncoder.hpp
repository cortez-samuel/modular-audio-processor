#ifndef ROTARYENCODER_HPP
#define ROTARYENCODER_HPP

#include "pico/stdlib.h"

template<uint64_t BOUNCING_TIME>
class RotaryEncoder {
    static const uint64_t BOUNCING_TIME_us = BOUNCING_TIME;

    static inline RotaryEncoder* instances[32];

public:
    using State_t = int;
    struct StateDetails_t {
        uint64_t startTime_us;
        uint64_t duration_us;
        State_t state;
    };

private:
    uint _pinA, _pinB;

    void (*posRotateCallback)(RotaryEncoder*, State_t nextState);
    bool posRotateCallbackEnabled;
    void (*negRotateCallback)(RotaryEncoder*, State_t nextState);
    bool negRotateCallbackEnabled;

    uint64_t t0;
    bool bouncing;
    State_t position;       // state of rotary encoder

public:
    RotaryEncoder() {
        _pinA   = 0;
        _pinB   = 0;

        posRotateCallback           = nullptr;
        posRotateCallbackEnabled    = false;
        negRotateCallback           = nullptr;
        negRotateCallbackEnabled    = false;

        t0          = 0;
        bouncing    = false;
        position    = 0;        
    }
    RotaryEncoder(uint pinA, uint pinB) {
        _pinA   = 0;
        _pinB   = 0;

        posRotateCallback           = nullptr;
        posRotateCallbackEnabled    = false;
        negRotateCallback           = nullptr;
        negRotateCallbackEnabled    = false;

        t0          = 0;
        bouncing    = false;
        position    = 0;

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

    inline void setCallback(void (*callback)(RotaryEncoder*, State_t), bool isNegRotateCallback, bool enabled) {
        if (isNegRotateCallback) {
            negRotateCallbackEnabled = enabled;
            negRotateCallback = callback;
        }
        else {
            posRotateCallbackEnabled = enabled;
            posRotateCallback = callback;
        }
    }

    inline State_t getState() const {
        return position;
    }
    inline void setState(State_t newPosition) {
        position = newPosition;
    }

    inline StateDetails_t getStateDetails() const {
        uint64_t duration = time_us_64() - t0;
        return {
            t0,
            duration,
            position
        };
    }

private:
    inline void updatePosition(int amount) {
        position += amount;
    }

private:
    void __time_critical_func(_GPIOIRQ)(uint gpio, uint32_t event_mask) {
        if (!isBouncing()) {
            if (gpio_get(_pinB)) {
                if (posRotateCallbackEnabled) posRotateCallback(this, position + 1);
                updatePosition(1);
            }
            else {
                if (negRotateCallbackEnabled) negRotateCallback(this, position - 1);
                updatePosition(-1);
            }
            t0 = time_us_64();
            bouncing = true;
        }

        gpio_acknowledge_irq(gpio, event_mask);
    }
    static void __time_critical_func(_clsGPIOIRQ)(uint gpio, uint32_t event_mask) {
        RotaryEncoder* instance = instances[gpio];
        instance->_GPIOIRQ(gpio, event_mask);
    }
};


#endif