#include "../libraries/adc.hpp"

#include "pico/stdlib.h"
#include <cstdio>

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



int main() {

    stdio_init_all();

    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);
    
    for (uint8_t i=6; i<14; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
    }


    float sample_frequency = 44100;
    ADC::init(sample_frequency, true, false, 1, customIRQ);
    uint64_t outputPeriod_us = (uint64_t)(1000000.0 / sample_frequency) >> 2;

    ADC::setActiveChannel(1);
    ADC::enableIRQ(true);
    ADC& adc1 = ADC::getActiveChannel();
    ADC::run(true);
    
    buffer[0] = 0; 
    buffer[1] = 0;
    while(1) {
        writeDigital(buffer[0] >> 4);
        sleep_us(outputPeriod_us);
        writeDigital((buffer[0] + buffer[1]) >> 5);
   }
}