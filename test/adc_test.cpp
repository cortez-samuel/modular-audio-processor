#include "../libraries/adc.hpp"

#include "pico/stdlib.h"
#include <cstdio>

int main() {

    stdio_init_all();

    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);

    ADC::init(750, true, false, 1, defaultADCRIQHandler);

    ADC::setActiveChannel(1);
    ADC::enableIRQ(true);
    ADC& adc1 = ADC::getActiveChannel();
    ADC::run(true);

    while(1) {
        if (adc1.newValue()) {
            printf("ADC_0 :: %f\n", adc1.trueValue());
        }
        sleep_ms(10);
    }
}