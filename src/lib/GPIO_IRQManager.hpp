#ifndef GPIO_IRQ_MANAGER_HPP
#define GPIO_IRQ_MANAGER_HPP

#include "pico/stdlib.h"

#include <cstdio>

class GPIO_IRQManager {
    static const uint PIN_COUNT = 32;
    static inline gpio_irq_callback_t pins[PIN_COUNT];

    static void __time_critical_func(defaultIRQ)(uint gpio, uint32_t event_mask) {
        gpio_acknowledge_irq(gpio, event_mask);
    }
    static void __time_critical_func(_IRQ)(uint gpio, uint32_t event_mask) {
        //printf("gpio: %u\n", gpio);
        gpio_irq_callback_t irqCallback = pins[gpio];
        irqCallback(gpio, event_mask);
    }

public:
    static inline void init() {
        for (uint i = 0; i < PIN_COUNT; i++) {
            pins[i] = defaultIRQ;
        }
    }

    static bool hasIRQ(uint gpio) {
        return pins[gpio] != defaultIRQ;
    }
    static void setIRQCallback(uint gpio, uint32_t event_mask, gpio_irq_callback_t irqCallback, bool enabled) {
        pins[gpio] = irqCallback;
        gpio_set_irq_enabled_with_callback(gpio, event_mask, enabled, _IRQ);
    }
    static void enableIRQCallback(uint gpio, uint32_t event_mask, bool enabled) {
        gpio_set_irq_enabled_with_callback(gpio, event_mask, enabled, _IRQ);
    }
};

#endif