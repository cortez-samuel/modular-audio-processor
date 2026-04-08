#include "pico/stdlib.h"

#include "../lib/RotaryEncoder.hpp"

#include <cstdio>

#define PIN_A 2
#define PIN_B 3


uint times_called = 0;

RotaryEncoder inst;

void callback(RotaryEncoder* inst, RotaryEncoder::State_t next) {
    RotaryEncoder::StateDetails_t details = inst->getStateDetails();
    printf("prev: %i\t duration: %f\t next: %i\n", details.state, details.duration_us/1000000.0, next);
}

int main() {
    stdio_init_all();

    inst.settings = {
        .debounceTime_us = 7500,
        .onInc = callback,
        .onIncEnabled = true,
        .onDec = callback,
        .onDecEnabled = true,
    };
    inst.begin(PIN_A, PIN_B);
    
    
    while(1) {
        tight_loop_contents();
    }
}