#include "../libraries/adc.hpp"

#include "pico/stdlib.h"
#include <cstdio>

/*n
void writeDigital(uint8_t D) {
    for (uint8_t b=0; b<8; b++) {
        gpio_put(13 - b, (D >> b) & 0b1);
    }
}

uint16_t buffer[2];
void customIRQ() {
    buffer[0] = buffer[1];
    buffer[1] = ADC::readFIFO();
}
*/


int main() {

    stdio_init_all();

    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);

    float sample_frequency = 1000;
    ADC::settings = {
        .fs = sample_frequency,
        .resolution = 12,
        .freeRun = true,
        .rrEnabled = true,
    };
    ADC adc0(0);
    ADC adc1(1);
    adc0.enable(true);
    adc1.enable(true);
    ADC::init(0);
    uint64_t outputPeriod_us = (uint64_t)(1000000.0 / sample_frequency) >> 2;

    ADC::run(true);

    sleep_ms(5000);
    
    printf("-----\n");
    printf("\tActive channel:\t%u\n", ADC::getActiveChannel());
    printf("\tadc0 channel:\t%u\n", adc0.getChannel());
    printf("\tadc0 enabled:\t%u\n", adc0._enabled);
    printf("\tRR:\t%02x", ADC::__activeChannelMask);
    printf("\tRunning:\t%u", ADC::__running);
    
    uint pin13Val = 1;
    while(1) {
        if (adc0.newValue()) {
            printf("0: %f\n", adc0.getTrue());
            //adc0.getTrue();
            gpio_put(13, pin13Val);
            pin13Val = 1-pin13Val;
        }   
        if (adc1.newValue()) {
            printf("1: %f\n", adc1.getTrue());
            //adc1.getTrue();
            gpio_put(13, pin13Val);
            pin13Val = 1-pin13Val;
        }    
    }
}