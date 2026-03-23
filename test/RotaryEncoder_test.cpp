#include "pico/stdlib.h"

#include <cstdio>

#define PIN_A 2
#define PIN_B 3


uint times_called = 0;

template<uint64_t BOUNCING_TIME>
struct RotaryEncoder {
    static const uint64_t BOUNCING_TIME_us = BOUNCING_TIME;

    uint64_t t0;
    bool bouncing;

    int position;

    RotaryEncoder() {
        t0 = 0;
        bouncing = false;
        position = 0;        
    }

    inline void on_turn() {
        if (gpio_get(PIN_B)) {
            position++;
        }
        else {
            position--;
        }
    }
    inline void on_turn_bounce() {
        uint64_t now = time_us_64();
        if ((now - t0) > BOUNCING_TIME_us) {
            bouncing = false;
        }
        if (!bouncing) {
            on_turn();
            t0 = now;
            bouncing = true;
        }
    }
};


RotaryEncoder<1000> inst;
void IRQ(uint gpio, uint32_t event_mask) {
    times_called++;

    inst.on_turn_bounce();

    gpio_acknowledge_irq(PIN_A, event_mask);
}


int main() {
    stdio_init_all();

    gpio_init(PIN_A);
    gpio_init(PIN_B);

    gpio_set_dir(PIN_A, 0);
    gpio_set_dir(PIN_B, 0);

    gpio_set_irq_enabled_with_callback(PIN_A, GPIO_IRQ_EDGE_RISE, true, IRQ);
    
    
    while(1) {
        printf("TimesCalled: %u\t Position: %i\r\n", times_called, inst.position);
        sleep_ms(100);
    }
}