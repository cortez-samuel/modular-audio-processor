#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "../libraries/adc.hpp"
#include "../libraries/I2S.h"
#include <cstdio>
#include <cstdint>

static const uint8_t ADC_CHANNEL = 0;
static const uint8_t PIN_ADC_CHANNEL = ADC_GET_CHANNEL_PIN(ADC_CHANNEL);
static const uint8_t PIN_I2S_Tx_SD = 0;
static const uint8_t PIN_I2S_Tx_BCLK = 2;
static const uint8_t PIN_I2S_Tx_WS = 3;
static const uint I2S_WS_FRAME_SIZE = 16;
static const float fs = 44100;
static const int _E = 15;
static const float _E_SCALE  = (float)(1 << _E);

static inline int16_t float_to_fix(float x) {
    if(x >=  1.0f){
        return  0x7FFF;
    }
    if(x <= -1.0f){
        return (int16_t)0x8000;
    }
    return (int16_t)(x * _E_SCALE);
}

static inline int16_t adc_raw_to_fix(uint16_t raw12) {
    float u_vol = raw12 * (3.3f / 4096.0f);
    float s_vol = (u_vol - 1.65f) / 1.65f;
    return float_to_fix(s_vol);
}

int main() {
    stdio_init_all();
    ADC::init(fs, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ADC_CHANNEL, true);
    ADC::setActiveChannel(ADC_CHANNEL);
    ADC::enableIRQ(true);
    ADC& adc0 = ADC::getActiveChannel();
    I2S_Tx i2sTx;
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, fs, I2S_WS_FRAME_SIZE);
    ADC::run(true);
    i2sTx.enable(true);
    int16_t lastSample = 0;
    while (true) {
        if (adc0.newValue()) {
            lastSample = adc_raw_to_fix(adc0.rawValue());
            uint32_t word = (uint32_t)(uint16_t)lastSample;
            i2sTx.queue(word, word);
        }
    }
}