#include "adc.hpp"

#include "hardware/irq.h"


void ADC::ADC_INIT() {
    adc_init();
}

ADC::ADC(uint8_t channel, float sample_freq_Hz) {
    static const float ADC_CLK_HZ = 48 * 1000 * 1000;
    adc_gpio_init(channel + 26);
    adc_select_input(channel);

    float div = ADC_CLK_HZ / sample_freq_Hz - 1.0f;
    adc_set_clkdiv(div);
}

ADC::~ADC() {}

void ADC::initFIFO(bool enable, bool dreq_enable, uint8_t threshold, bool shift, bool err) {
    adc_fifo_setup(enable, dreq_enable, threshold, err, shift);
}

void ADC::initIRQ(irq_handler_t handler, bool enable) {
    irq_set_exclusive_handler(ADC_IRQ_FIFO, handler);
    adc_irq_set_enabled(enable);
    irq_set_enabled(ADC_IRQ_FIFO, enable);
}

void ADC::clearFIFO() {
    adc_fifo_drain();
}

void ADC::setActive() {
}

void ADC::readOnce()
{
    value = adc_read();
}
void ADC::readContinuous(bool enable) {
    adc_run(enable);
}

static const float conversion_factor = 3.3f / (1 << 12);
uint16_t ADC::rawValue() const {
    return value;
}
float ADC::trueValue() const {
    return value * conversion_factor;
}

