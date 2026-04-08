#ifndef ROTARYENCODER_HPP
#define ROTARYENCODER_HPP

#include "pico/stdlib.h"

#include "GPIO_IRQManager.hpp"

class RotaryEncoder {
    static inline RotaryEncoder* instances[32];

public:
    using State_t = int;
    struct StateDetails_t {
        uint64_t startTime_us;
        uint64_t duration_us;
        State_t state;
    };

public:
    using RotaryEncoderCallback_t = void (*)(RotaryEncoder*, State_t);
    struct Settings_t {
        uint64_t debounceTime_us;
        RotaryEncoderCallback_t onInc;
        bool onIncEnabled;
        RotaryEncoderCallback_t onDec;
        bool onDecEnabled;
    };

    inline static constexpr Settings_t defaultSettings {
        .debounceTime_us    = 10000,
        .onInc              = nullptr,
        .onIncEnabled       = false,
        .onDec              = nullptr,
        .onDecEnabled       = false,
    };
    Settings_t settings; 

private:
    uint _pinA, _pinB;

    uint64_t t0;
    bool bouncing;
    State_t position;       // state of rotary encoder

public:
    RotaryEncoder() {
        t0          = 0;
        bouncing    = false;
        position    = 0;        
        
        settings    = defaultSettings;
    }

public:
    void begin(uint pinA, uint pinB) {
        _pinA = pinA;
        _pinB = pinB;

        gpio_init(pinA);
        gpio_init(pinB);

        gpio_set_dir(pinA, 0);
        gpio_set_dir(pinB, 0);

        instances[pinA] = this;
        instances[pinB] = this;

        GPIO_IRQManager::setIRQCallback(pinA, GPIO_IRQ_EDGE_RISE, _clsGPIOIRQ, true);
    }

public:
    inline bool isBouncing() {
        bouncing = (time_us_64() - t0) < settings.debounceTime_us;
        return bouncing;
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
                if (settings.onIncEnabled && settings.onInc != nullptr) settings.onInc(this, position + 1);
                updatePosition(1);
            }
            else {
                if (settings.onDecEnabled && settings.onDec != nullptr) settings.onDec(this, position - 1);
                updatePosition(-1);
            }
            t0 = time_us_64();
            bouncing = true;
        }

        gpio_acknowledge_irq(gpio, event_mask);
    }
    static void __time_critical_func(_clsGPIOIRQ)(uint gpio, uint32_t event_mask) {
        RotaryEncoder* instance = instances[gpio];
        if (instance == nullptr) {
            return;
        }
        instance->_GPIOIRQ(gpio, event_mask);
    }
};


#endif