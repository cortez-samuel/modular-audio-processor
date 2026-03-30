#include "pico/stdlib.h"

#include "../libraries/RotaryEncoder.hpp"

#include <cstdio>

#define PIN_A 2
#define PIN_B 3


uint times_called = 0;

RotaryEncoder<1000> inst;

void callback(RotaryEncoder<1000>* inst, RotaryEncoder<1000>::State_t next) {
    RotaryEncoder<1000>::StateDetails_t details = inst->getStateDetails();
    printf("prev: %i\t duration: %f\t next: %i\n", details.state, details.duration_us/1000000.0, next);
}

int main() {
    stdio_init_all();

    inst.setCallback(callback, false, true);
    inst.setCallback(callback, true, true);

    inst.begin(PIN_A, PIN_B);
    
    
    while(1) {
        tight_loop_contents();
    }
}