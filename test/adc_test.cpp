#include "../libraries/adc.hpp"

#include "pico/stdlib.h"
#include <cstdio>

int main() {

    stdio_init_all();

    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);
    gpio_init(7);
    gpio_set_dir(7, GPIO_OUT);

    
    ADC::init(750, true, false, 1, defaultADCRIQHandler);

    ADC::setActiveChannel(1);
    ADC::enableIRQ(true);
    ADC& adc1 = ADC::getActiveChannel();
    ADC::run(true);
    
    bool pin7 = false;
    while(1) {
        
        if (adc1.newValue()) {
            printf("ADC_0 :: %f\n", adc1.trueValue());
            gpio_put(7, pin7);
            pin7 = !pin7;
        }
        sleep_ms(13);
    }
}