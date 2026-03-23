#include "pico/stdlib.h"

#include "../libraries/RotaryEncoder.hpp"

#include <cstdio>

#define PIN_A 2
#define PIN_B 3


uint times_called = 0;

RotaryEncoder<1000> inst;

int main() {
    stdio_init_all();

    inst.begin(PIN_A, PIN_B);
    
    
    while(1) {
        printf("TimesCalled: %u\t Position: %i\r\n", times_called, inst.getPosition());
        sleep_ms(100);
    }
}