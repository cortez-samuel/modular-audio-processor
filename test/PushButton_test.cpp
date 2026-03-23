#include "pico/stdlib.h"

#include "../libraries/PushButton.hpp"

#include <cstdio>


uint times_called = 0;


int main() {
    stdio_init_all();
    
    PushButton<10000> pushButton;
    pushButton.begin(0);

    uint timesPressed = 0;

    while(1) {
        if (pushButton.wasPressed()) timesPressed++;
        printf("TimesCalled: %u\t Times Pressed: %i\r\n", times_called, timesPressed);
        sleep_ms(100);
    }
}