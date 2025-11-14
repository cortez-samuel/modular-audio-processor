#ifndef ADC_HPP
#define ADC_HPP

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"


void defaultADCRIQHandler();

class ADC {
public:
    static void ADC_INIT();
    ADC(uint8_t channel, float sample_freq);
    ~ADC();

public:
    void initFIFO(bool enable, bool dreq_enable, uint8_t threshold, bool shift = false, bool err = false); 
    void initIRQ(irq_handler_t handler, bool enable);

    void setActive();
    void readOnce();
    void readContinuous(bool enable);

    uint16_t rawValue() const;
    float trueValue() const;

    void clearFIFO();

private:
    uint16_t value;
};


#endif // ADC_HPP