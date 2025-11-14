#include "adc.hpp"

#include "hardware/irq.h"


// OBJECT STUFF
ADC::ADC(uint8_t channel): m_value(0), m_channel(channel), m_newValue(false) {
    adc_gpio_init(channel + 26);
}

bool ADC::newValue() const {
    return m_newValue;
}

void ADC::setRawValue(uint16_t raw){
    m_newValue = true;
    m_value = raw;
}

static const float conversion_factor = 3.3f / (1 << 12);
uint16_t ADC::rawValue() {
    m_newValue = false;
    return m_value;
}
float ADC::trueValue() {
    m_newValue = false;
    return m_value * conversion_factor;
}

uint8_t ADC::getChannel() const {
    return m_channel;
}


static uint8_t activeChannel;
static uint8_t runningChannel;
static ADC adcChannels[4] = {
    ADC(0),
    ADC(1),
    ADC(2),
    ADC(3),
};
// OVERALL ADC STUFF
void ADC::init(float sample_freq, 
        bool fifo_enable, bool dreq_enable, uint16_t threshold, 
        irq_handler_t irq_handler) {
    // sample_freq >= 750

    adcChannels[0] = ADC(0);
    adcChannels[1] = ADC(1);
    adcChannels[2] = ADC(2);
    adcChannels[3] = ADC(3); 

    
    adc_init();
    ADC::setSampleFrequency(sample_freq);

    ADC::setupFIFO(fifo_enable, dreq_enable, threshold);
    ADC::setupIRQHandler(irq_handler);
}

ADC& ADC::getADCChannel(uint8_t channel) {
    return adcChannels[channel];
}

ADC& ADC::getActiveChannel() {
    return ADC::getADCChannel(activeChannel);
}

void ADC::setActiveChannel(uint8_t channel) {
    activeChannel = channel;
    adc_select_input(activeChannel);
}

void ADC::setActiveChannel(const ADC &adc) {
    activeChannel = adc.getChannel();
    adc_select_input(activeChannel);
}

uint16_t ADC::read() {
    uint16_t raw = adc_read();
    adcChannels[activeChannel].setRawValue(raw);
    return raw;
}

uint16_t ADC::read(ADC& adc) {
    // will also place data in FIFO buffer
    ADC::setActiveChannel(adc);
    return ADC::read();
}

void ADC::run(bool enable) {
    adc_run(enable);
    runningChannel = activeChannel;
}
void ADC::run(ADC& adc, bool enable) {
    ADC::setActiveChannel(adc);
    ADC::run(enable);
}

static const float ADC_CLK_HZ = 48 * 1000 * 1000;
void ADC::setSampleFrequency(float sample_freq) {
    float div = ADC_CLK_HZ / sample_freq - 1.0f;
    adc_set_clkdiv(div);
}

void ADC::setupFIFO(bool enable, bool dreq_enable, uint16_t threshold) {
    adc_fifo_setup(enable, dreq_enable, threshold, false, false);
}
void ADC::clearFIFO() {
    if (adc_fifo_get_level() == 0) {return;}
    adc_fifo_drain();
}
uint16_t ADC::readFIFO() {
    uint16_t value = adc_fifo_get();
    ADC::getActiveChannel().setRawValue(value);
    return value;
}

void ADC::setupIRQHandler(irq_handler_t handler) {
    irq_set_exclusive_handler(ADC_IRQ_FIFO, handler);
}

void ADC::enableIRQ(bool enable) {
    adc_irq_set_enabled(enable);
    irq_set_enabled(ADC_IRQ_FIFO, enable);
}


//bool pin13 = true;
#include <cstdio>
void defaultADCRIQHandler() {
    ADC::readFIFO();
    //gpio_put(13, pin13);
    //pin13 = !pin13;
}