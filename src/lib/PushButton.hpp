#ifndef PUSHBUTTON_HPP
#define PUSHBUTTON_HPP

#include "pico/stdlib.h"

#include "GPIO_IRQManager.hpp"

class PushButton {
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

public:
    using PushButtonCallback_t = void (*)(PushButton*, State_t);
    struct Settings_t {
        uint64_t debounceTime_us;
        PushButtonCallback_t onDown;
        bool onDownEnabled;
        PushButtonCallback_t onUp;
        bool onUpEnabled;
    };
    
    inline static constexpr Settings_t defaultSettings {
        .debounceTime_us    = 50000,
        .onDown             = nullptr,
        .onDownEnabled      = false,
        .onUp               = nullptr,
        .onUpEnabled        = false,
    };
    Settings_t settings;

private:
    uint _pin;

    uint64_t t0;
    bool bouncing;
    State_t state;

public:
    PushButton() {
        t0          = 0;
        bouncing    = false;
        state       = UP;

        settings    = defaultSettings;
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
        bouncing = (time_us_64() - t0) < settings.debounceTime_us;
        return bouncing;
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
    inline void onDown() {
        state = DOWN;
    }

    inline void onUp() {
        state = UP;
    }

public:
    void __time_critical_func(_GPIOIRQ)(uint gpio, uint32_t event_mask) {
        if (!isBouncing()) {
            if ((event_mask & GPIO_IRQ_EDGE_RISE) && state == UP) {    // push button pressed
                if (settings.onDownEnabled && (settings.onDown != nullptr)) { 
                    settings.onDown(this, DOWN); 
                }
                onDown();
            }
            else if ((event_mask & GPIO_IRQ_EDGE_FALL) && state == DOWN) { // push button released
                if (settings.onUpEnabled && (settings.onUp != nullptr)) { 
                    settings.onUp(this, UP); 
                }
                onUp();
            }
            t0 = time_us_64();
            bouncing = true;
        }

        gpio_acknowledge_irq(gpio, event_mask);
    }
    static void __time_critical_func(_clsGPIOIRQ)(uint gpio, uint32_t event_mask) {
        PushButton* instance = instances[gpio];
        if (instance == nullptr) {
            return;
        }
        instance->_GPIOIRQ(gpio, event_mask);
    }
};

#endif