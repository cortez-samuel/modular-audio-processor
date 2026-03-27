#ifndef PUSHBUTTON_HPP
#define PUSHBUTTON_HPP

#include "pico/stdlib.h"

#include "GPIO_IRQManager.hpp"

template<uint64_t BOUNCING_TIME>
struct PushButton {
    static const uint64_t BOUNCING_TIME_us = BOUNCING_TIME;

    static inline PushButton* instances[32];

public:
    enum State_t {
        UP,
        DOWN
    };
    struct StateDetails_t {
        uint64_t startTime_us;
        uint64_t duration_us;
        State_t state;
    };

private:
    uint _pin;
    
    void (*onDownCallback)(PushButton*, State_t);
    bool onDownCallbackEnabled;
    void (*onUpCallback)(PushButton*, State_t);
    bool onUpCallbackEnabled;

    uint64_t t0;
    bool bouncing;
    State_t state;

public:
    PushButton() {
        t0 = 0;
        bouncing = false;
        state = UP;
    }

public:
    void begin(uint pin) {
        _pin = pin;

        gpio_init(pin);
        
        gpio_set_dir(pin, GPIO_IN);

        instances[pin] = this;

        const uint32_t event_mask = GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE;
        GPIO_IRQManager::setIRQCallback(pin, event_mask, _clsGPIOIRQ, true);
    }

public:
    inline bool isBouncing() {
        bouncing = (time_us_64() - t0) < BOUNCING_TIME_us;
        return bouncing;
    }

    inline void setCallback(void (*callback)(PushButton*, State_t), bool isOnUpCallback, bool enabled) {
        if (isOnUpCallback) {
            onUpCallbackEnabled = enabled;
            onUpCallback = callback;
        }
        else {
            onDownCallbackEnabled = enabled;
            onDownCallback = callback;
        }
    }

    inline State_t getState() const {
        return state;
    }

    inline StateDetails_t getStateDetails() const {
        uint64_t duration = time_us_64() - t0;
        return {
            t0,
            duration,
            state
        };
    }

public:
    void onDown() {
        state = DOWN;
    }

    void onUp() {
        state = UP;
    }

public:
    void __time_critical_func(_GPIOIRQ)(uint gpio, uint32_t event_mask) {
        if (!isBouncing()) {
            if (event_mask & GPIO_IRQ_EDGE_RISE) {
                if (onDownCallbackEnabled) onDownCallback(this, DOWN);
                onDown();
            }
            else if (event_mask & GPIO_IRQ_EDGE_FALL) {
                if (onUpCallbackEnabled) onUpCallback(this, UP);
                onUp();
            }
            t0 = time_us_64();
            bouncing = true;
        }

        gpio_acknowledge_irq(gpio, event_mask);
    }
    static void __time_critical_func(_clsGPIOIRQ)(uint gpio, uint32_t event_mask) {
        PushButton* instance = instances[gpio];
        instance->_GPIOIRQ(gpio, event_mask);
    }
};

#endif