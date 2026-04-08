#include "pico/stdlib.h"

#include "../lib/PushButton.hpp"
#include "../lib/RotaryEncoder.hpp"
#include "../lib/GPIO_IRQManager.hpp"

#include <cstdio>


uint times_called = 0;

void callback(PushButton* inst, PushButton::State_t next) {
    static uint timesPressed = 0;
    PushButton::StateDetails_t details = inst->getStateDetails();
    if (next == PushButton::DOWN) {
        timesPressed++;
        printf("times pressed: %u\n", timesPressed);
    }
    else if (next == PushButton::UP) {
        printf("held for: %fs\n", details.duration_us/1000000.0f);
    }
}

void encCallback(RotaryEncoder* inst, RotaryEncoder::State_t next) {
    printf("rotated\t pos: %i\n", next);
}

int main() {
    stdio_init_all();

    GPIO_IRQManager::init();

    RotaryEncoder inst;
    inst.settings = {
        .debounceTime_us = 10000,
        .onInc = encCallback,
        .onIncEnabled = true,
        .onDec = encCallback,
        .onDecEnabled = true,
    };
    PushButton pushButton;
    pushButton.settings.onDown = callback;
    pushButton.settings.onDownEnabled = true;
    pushButton.settings.onUp = callback;
    pushButton.settings.onUpEnabled = true;
    pushButton.begin(12);
    inst.begin(1,6);

    uint timesPressed = 0;

    while(1) {
        tight_loop_contents();
    }
}