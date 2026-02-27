#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "../libraries/adc.hpp"
#include "../libraries/I2S.h"
#include <cstdio>
#include <cstdint>

static const uint8_t ADC_CHANNEL        = 0;
static const uint8_t PIN_ADC_CHANNEL    = ADC_GET_CHANNEL_PIN(ADC_CHANNEL);
static const uint8_t PIN_I2S_Tx_SD      = 0;
static const uint8_t PIN_I2S_Tx_BCLK    = 2;
static const uint8_t PIN_I2S_Tx_WS      = 3;
static const uint I2S_WS_FRAME_SIZE     = 16;
static const float fs = 44100;

// ---------------------------------------------------------------------------
// Fixed-point helpers  (_E = 15  =>  1 sign bit, 15 fractional bits)
//   Range : [-1.0,  +0.999969...]
//   LSB   :  1 / 32768  ~  0.0000305
// ---------------------------------------------------------------------------
static const int   _E        = 15;
static const float _E_SCALE  = (float)(1 << _E);   // 32768.0f

// float in [-1, 1)  -->  int16_t  (saturating)
static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  0x7FFF;
    if (x <= -1.0f) return (int16_t)0x8000;
    return (int16_t)(x * _E_SCALE);
}

// ---------------------------------------------------------------------------
// ADC raw (12-bit, unsigned) --> signed fi16 (_E=15)
//
//   raw12  [0, 4095]
//   u_vol  = raw12 * 3.3 / 4096        -- unsigned voltage [0.0, 3.3) V
//   s_vol  = (u_vol - 1.65) / 1.65     -- signed voltage   [-1.0, 1.0)
//   sample = float_to_fix(s_vol)        -- int16_t fixed-point
//
// Assumes the analog input is biased to 1.65 V (mid-supply).
// ---------------------------------------------------------------------------
static inline int16_t adc_raw_to_fix(uint16_t raw12) {
    float u_vol = raw12 * (3.3f / 4096.0f);
    float s_vol = (u_vol - 1.65f) / 1.65f;
    return float_to_fix(s_vol);
}

int main() {
    stdio_init_all();
        
    // Initialize the ADC
    ADC::init(fs, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ADC_CHANNEL, true);
    ADC::setActiveChannel(ADC_CHANNEL);
    ADC::enableIRQ(true);
    ADC& adc0 = ADC::getActiveChannel();

    // Initialize I2S_Tx
    I2S_Tx i2sTx;
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, fs, I2S_WS_FRAME_SIZE);

    // Start ADC and I2S
    ADC::run(true);
    i2sTx.enable(true);

    int16_t lastSample = 0;
    while (true) {
        if (adc0.newValue()) {
            // Convert ADC raw value to signed fixed-point sample
            lastSample = adc_raw_to_fix(adc0.rawValue());

            // Cast to uint16_t first to preserve the bit pattern, then widen to
            // uint32_t so queue() receives the correct 16-bit word in the LSBs.
            // queue() will left-shift by (32 - WS_frame_size) = 16 before sending,
            // placing the signed 16-bit value in the correct position for the DAC.
            uint32_t word = (uint32_t)(uint16_t)lastSample;
            i2sTx.queue(word, word);
        }
    }
}