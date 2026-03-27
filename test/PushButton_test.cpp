#include "pico/stdlib.h"

#include "../libraries/PushButton.hpp"
#include "../libraries/RotaryEncoder.hpp"
#include "../libraries/GPIO_IRQManager.hpp"

#include <cstdio>


uint times_called = 0;

void callback(PushButton<10000>* inst, PushButton<10000>::State_t next) {
    static uint timesPressed = 0;
    PushButton<10000>::StateDetails_t details = inst->getStateDetails();
    if (next == PushButton<10000>::DOWN) {
        timesPressed++;
        printf("times pressed: %u\n", timesPressed);
    }
    else if (next == PushButton<10000>::UP) {
        printf("held for: %fs\n", details.duration_us/1000000.0f);
    }
}

void encCallback(RotaryEncoder<10000>* inst, RotaryEncoder<10000>::State_t next) {
    printf("rotated\t pos: %i\n", next);
}

int main() {
    stdio_init_all();

    GPIO_IRQManager::init();

        RotaryEncoder<10000> inst;
    inst.setCallback(encCallback, false, true);
    inst.setCallback(encCallback, true, true);
    PushButton<10000> pushButton;
    pushButton.setCallback(callback, false, true);
    pushButton.setCallback(callback, true, true);
    pushButton.begin(12);
        inst.begin(1,6);


    uint timesPressed = 0;

    while(1) {
        tight_loop_contents();
    }
}