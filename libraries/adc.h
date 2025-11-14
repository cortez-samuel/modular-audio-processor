#ifndef ADC_H
#define ADC_H

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"


class ADC {
public:
    ADC();
    ~ADC();

public:
    void init();
}

#endif // ADC_H