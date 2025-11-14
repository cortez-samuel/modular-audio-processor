#ifndef ADC_HPP
#define ADC_HPP

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"


void defaultADCRIQHandler();

class ADC {
public:
    static void init(float sample_freq, 
        bool fifo_enable, bool dreq_enable, uint16_t threshold, 
        irq_handler_t irq_handler);

    static ADC& getADCChannel(uint8_t channel);

    static ADC& getActiveChannel();
    static void setActiveChannel(uint8_t channel);
    static void setActiveChannel(const ADC& adc);

    static uint16_t read();
    static uint16_t read(ADC& adc);

    static void run(bool enable);
    static void run(ADC& adc, bool enable);

    static void setSampleFrequency(float sample_freq);

    static void setupFIFO(bool enable, bool dreq_enable, uint16_t threshold);
    static void clearFIFO();
    static uint16_t readFIFO();

    static void setupIRQHandler(irq_handler_t handler);
    static void enableIRQ(bool enable);


public:
    ADC(uint8_t channel);

public:
    bool newValue() const;

    void setRawValue(uint16_t raw);

    uint16_t rawValue();
    float trueValue();

    uint8_t getChannel() const;
private:
    uint16_t m_value;
    uint8_t m_channel;
    bool m_newValue;
};

#endif // ADC_HPP