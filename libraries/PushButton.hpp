#ifndef PUSHBUTTON_HPP
#define PUSHBUTTON_HPP

#include "pico/stdlib.h"

template<uint64_t BOUNCING_TIME>
struct PushButton {
    static const uint64_t BOUNCING_TIME_us = BOUNCING_TIME;

    static inline PushButton* instances[32];

    uint _pin;
    
    uint64_t t0;
    bool bouncing;
    enum State_t {
        UP = 0,
        DOWN
    } state;

    bool _press, _release;
    uint _timeOfPress, _timeOfRelease;

public:
    PushButton() {
        t0 = 0;
        bouncing = false;
        state = UP;

        _press = false;
        _timeOfPress = 0;
        _release = false;
        _timeOfRelease = 0;
    }

public:
    void begin(uint pin) {
        _pin = pin;

        gpio_init(pin);
        
        gpio_set_dir(pin, GPIO_IN);

        gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, _clsGPIOIRQ);

        instances[pin] = this;
    }

public:
    inline bool isBouncing() {
        bouncing = (time_us_64() - t0) < BOUNCING_TIME_us;
        return bouncing;
    }
    inline bool isDown() const {
        return state == DOWN;
    }
    inline bool isUp() const {
        return state == UP;
    }

    inline bool wasPressed() {
        bool ret = _press;
        _press = false;
        return ret;
    }
    inline uint getTimeOfLastPress() {
        return _timeOfPress;
    }
    inline bool wasReleased() {
        bool ret = _release;
        _release = false;
        return ret;
    }
    inline uint getTimeOfLastRelease() {
        return _timeOfRelease;
    }

public:
    void onDown() {
        state = DOWN;
        _press = true;
        _timeOfPress = time_us_32();
    }
    void onDown_bouncing() {
        uint64_t now = time_us_64();

        if (!isBouncing() && state == UP) {
            onDown();
            t0 = now;
            bouncing = true;
        }
    }

    void onUp() {
        state = UP;
        _release = true;
        _timeOfRelease = time_us_32();
    }
    void onUp_bouncing() {
        uint64_t now = time_us_64();

        if (!isBouncing() && state == DOWN) {
            onUp();
            t0 = now;
            bouncing = true;
        }
    }

public:
    void __time_critical_func(_GPIOIRQ)(uint gpio, uint32_t event_mask) {
        if (event_mask & GPIO_IRQ_EDGE_RISE) {
            onDown_bouncing();
        }
        else if (event_mask & GPIO_IRQ_EDGE_FALL) {
            onUp_bouncing();
        }
        gpio_acknowledge_irq(gpio, event_mask);
        return;
    }
    static void __time_critical_func(_clsGPIOIRQ)(uint gpio, uint32_t event_mask) {
        PushButton* instance = instances[gpio];
        instance->_GPIOIRQ(gpio, event_mask);
    }
};

#endif