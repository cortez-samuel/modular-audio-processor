#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "../libraries/I2S.h"
#include "../libraries/oled.h"
#include "../libraries/adc.hpp"
#include "../libraries/fft.hpp"

    // LED PIN
static const uint LED_PIN           = 13;
static volatile uint LED_PIN_VALUE  = 0;

    // OLED
static const auto    SPI_INSTANCE   = spi1;
static const uint8_t PIN_SCK        = 10;
static const uint8_t PIN_TX         = 11;
static const uint8_t PIN_RST        = 28;
static const uint8_t PIN_DC         = 29;
static const uint8_t PIN_CS         = 24;

    // SHARED STUFF
static volatile uint8_t DOWNSAMPLE_FACTOR   = 16;
static const uint8_t    SHARED_BUFFER_WIDTH = 64;
static const uint8_t    SHARED_BUFFER_DEPTH = 128;
// store int16_t bit pattern in the low 16 bits.
static queue_t sharedQueue;

    // I2S TX
static const uint8_t PIN_I2S_Tx_SD   = 0;
static const uint8_t PIN_I2S_Tx_BCLK = 2;
static const uint8_t PIN_I2S_Tx_WS   = 3;
static I2S_Tx i2sTx;

    // I2S RX
static const uint8_t PIN_I2S_Rx_SD   = 7;
static const uint8_t PIN_I2S_Rx_BCLK = 8;
static const uint8_t PIN_I2S_Rx_WS   = 9;
static I2S_Rx i2sRx;
static const uint32_t reservedMemDepth = 128;
static uint32_t reservedMem[reservedMemDepth * RxPingPong::WIDTH];

    // I2S GENERAL
static const uint  I2S_WS_FRAME_WIDTH = 16;
static const float fs                 = 44100;

    // ADC / USER INPUT
static const uint8_t ALPHA_ADC_CHANNEL     = 0;
static const uint8_t PIN_ALPHA_ADC_CHANNEL = ADC_GET_CHANNEL_PIN(ALPHA_ADC_CHANNEL);
static const uint8_t CHANGE_MODE           = 12;
static const int     ALPHA_AVG_WINDOW      = 16;
static float alphaBuffer[ALPHA_AVG_WINDOW] = {0};
int alphaIndex = 0;

    // FILTERS
static float volatile alpha          = 0.0f;
static float alphaMin                = 0.033f;
static float alphaMax                = 1.0f;
static float (*currentFilter)(float) = nullptr;
static float filterOutput            = 0.0f;
static float Pass(float x);
static float LowPass(float x);
static float HighPass(float x);

enum class Mode { Pass, Lowpass, Highpass, FFT };
static Mode mode = Mode::Pass;
static const float _E_SCALE = 32768.0f;

static inline float fix_to_float(int16_t x) {
    return (float)x / _E_SCALE;
}

// float in [-1, 1)  ->  int16_t  (saturating)
static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  0x7FFF;
    if (x <= -1.0f) return (int16_t)0x8000;
    return (int16_t)(x * 32767.0f);
}

static inline float clamp_f(float lo, float x, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

// CORE 1 (OLED DISPLAY) MAIN
void core1_entry() {
    // Initialize OLED
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX,  GPIO_FUNC_SPI);
    OLED oled(SPI_INSTANCE, PIN_CS, PIN_DC, PIN_RST, 128, 64);
    sleep_ms(500);
    if (!oled.begin(10 * 1000 * 1000)) {
        while (true) {
            gpio_put(LED_PIN, 1); sleep_ms(500);
            gpio_put(LED_PIN, 0); sleep_ms(500);
        }
    }

    // Startup splash
    oled.clearDisplay();
    oled.setCursor(10, 5);
    oled.setTextSize(2);
    oled.setTextColor(true);
    oled.print("Modular \n Audio \n Processor");
    oled.display();
    sleep_ms(2000);

    // Circular buffer – stores int16_t bit-patterns in the low 16 bits of uint32_t.
    static const int CIRCULAR_BUFFER_SIZE = 512;
    uint32_t circularBuffer[CIRCULAR_BUFFER_SIZE] = {0};
    int bufferWriteIndex = 0;

    uint32_t lastDisplayTime = 0;
    const uint32_t DISPLAY_INTERVAL_MS = 33; // ~30 fps

    while (true) {
        // Drain the inter-core queue
        uint32_t word;
        while (queue_try_remove(&sharedQueue, &word)) {
            circularBuffer[bufferWriteIndex] = word;
            bufferWriteIndex = (bufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
        }

        uint32_t now = time_us_32() / 1000;
        if (now - lastDisplayTime >= DISPLAY_INTERVAL_MS) {
            lastDisplayTime = now;
            oled.clearDisplay();

            if (mode == Mode::FFT) {
                fi16 fftIn[256];
                for (int i = 0; i < 256; i++) {
                    fftIn[i] = (fi16)(int16_t)(uint16_t)circularBuffer[i];
                }
                int32_t mean = 0;
                for (int i = 0; i < 256; i++) { mean += fftIn[i]; }
                mean >>= 8;
                for (int i = 0; i < 256; i++) { fftIn[i] -= (fi16)mean; }
                fi16_32 fftOut[256];
                fft_fixed(fftIn, fftOut);
                fi16_32 fftOut_max = 1;
                for (int i = 0; i < 128; i++) {
                    if (fftOut[i] > fftOut_max) fftOut_max = fftOut[i];
                }
                bool       max_gt_64k  = fftOut_max > 65535;
                uint32_t   scale       = max_gt_64k
                                             ? (uint32_t)(fftOut_max / 65535 + 1)
                                             : (uint32_t)(65535 / fftOut_max);

                for (int x = 0; x < 128; x++) {
                    uint16_t dp = (uint16_t)(max_gt_64k
                                                 ? (fftOut[x] / scale)
                                                 : (fftOut[x] * scale));
                    uint8_t y = 63 - (uint8_t)(dp >> 10);
                    for (int row = 63; row > (int)y; row--) {
                        oled.drawPixel(x, row, true);
                    }
                }

            }
            else {
                for (int x = 0; x < 128; x++) {
                    if (x % 4 < 2){
                        oled.drawPixel(x, 32, true);
                    }
                }
                int lastY = 32;
                for (int x = 0; x < 128; x++) {
                    int bufIdx = (bufferWriteIndex
                                  - CIRCULAR_BUFFER_SIZE
                                  + (x * CIRCULAR_BUFFER_SIZE) / 128
                                  + CIRCULAR_BUFFER_SIZE) % CIRCULAR_BUFFER_SIZE;
                    int16_t s = (int16_t)(uint16_t)circularBuffer[bufIdx];
                    uint16_t offset_bin = (uint16_t)s ^ 0x8000u;
                    uint8_t  y          = 63 - (uint8_t)(offset_bin >> 10);
                    if (y > 63) y = 63;
                    if (x > 0) {
                        int diff = (int)y - lastY;
                        if (diff > 0) {
                            for (int dy = lastY; dy <= (int)y; dy++)
                                oled.drawPixel(x, dy, true);
                        }
                        else if (diff < 0) {
                            for (int dy = lastY; dy >= (int)y; dy--)
                                oled.drawPixel(x, dy, true);
                        }
                    }

                    oled.drawPixel(x, y, true);
                    lastY = y;
                }
            }

            // Mode label (top-left)
            oled.setTextSize(1);
            oled.setTextColor(true);
            oled.setCursor(0, 0);
            switch (mode) {
                case Mode::Pass:     oled.print("SRC"); break;
                case Mode::Lowpass:  oled.print("LPF"); break;
                case Mode::Highpass: oled.print("HPF"); break;
                case Mode::FFT:      oled.print("FFT"); break;
            }

            // Alpha value (top-right, filters only)
            if (mode != Mode::Pass && mode != Mode::FFT) {
                char buf[12];
                snprintf(buf, sizeof(buf), "a=%.2f", (float)alpha);
                oled.setCursor(128 - 6 * 6, 0);
                oled.print(buf);
            }

            oled.display();
        }

        sleep_us(100);
    }
}


// CORE 0 (IO / FILTER) MAIN
int main() {
    stdio_init_all();

    // LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Mode-change push button
    gpio_init(CHANGE_MODE);
    gpio_set_dir(CHANGE_MODE, GPIO_IN);
    gpio_pull_down(CHANGE_MODE);

    // Inter-core queue: element = uint32_t holding an int16_t bit-pattern.
    queue_init(&sharedQueue, sizeof(uint32_t), 256);

    multicore_launch_core1(core1_entry);

    // I2S init
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, fs, I2S_WS_FRAME_WIDTH);
    i2sRx.setReservedMem(reservedMem, reservedMemDepth);
    i2sRx.init(PIN_I2S_Rx_BCLK, PIN_I2S_Rx_WS, PIN_I2S_Rx_SD, fs, I2S_WS_FRAME_WIDTH);
    i2sTx.enable(true);
    i2sRx.enable(true);

    // Alpha-control ADC init
    ADC::init(10, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ALPHA_ADC_CHANNEL, true);
    ADC::setActiveChannel(ALPHA_ADC_CHANNEL);
    ADC::enableIRQ(true);
    ADC& adc0 = ADC::getActiveChannel();

    gpio_init(6);
    gpio_set_dir(6, GPIO_IN);
    gpio_pull_down(6);
    currentFilter = Pass;
    mode = Mode::Pass;

    ADC::run(true);
    uint32_t downsampleCounter = 0;

    while (true) {
    static bool lastButtonState = false;
        static uint32_t lastDebounceTime = 0;
        const uint32_t DEBOUNCE_MS = 150;
        bool buttonState = gpio_get(CHANGE_MODE);
        uint32_t now = time_us_32() / 1000;
        if (buttonState && !lastButtonState) {
            if (now - lastDebounceTime > DEBOUNCE_MS) {
                lastDebounceTime = now;
                switch (mode) {
                    case Mode::Pass: mode = Mode::Lowpass;  currentFilter = LowPass; break;
                    case Mode::Lowpass: mode = Mode::Highpass; currentFilter = HighPass; break;
                    case Mode::Highpass: mode = Mode::FFT; currentFilter = Pass; break;
                    case Mode::FFT: mode = Mode::Pass; currentFilter = Pass; break;
                }
            }
        }
        lastButtonState = buttonState;
        if (adc0.newValue()) alpha = clamp_f(alphaMin, adc0.trueValue() / 3.3f, alphaMax);
        if (alpha == alphaMin) alpha = 0.0f;
        uint32_t rxBuf[reservedMemDepth];
        if (i2sRx.readBuffer(rxBuf)) {
            for (int i = 0; i < (int)reservedMemDepth; i++) {
                int16_t raw_sample = (int16_t)(uint16_t)rxBuf[i];
                float s_vol = fix_to_float(raw_sample);
                float filtered_vol = currentFilter(s_vol);
                filtered_vol = clamp_f(-1.0f, filtered_vol, 1.0f);
                int16_t out_sample = float_to_fix(filtered_vol);
                uint32_t tx_word = (uint32_t)(uint16_t)out_sample;
                i2sTx.queue(tx_word, tx_word);
                downsampleCounter++;
                if (downsampleCounter >= DOWNSAMPLE_FACTOR) {
                    downsampleCounter = 0;
                    uint32_t q_word = (uint32_t)(uint16_t)out_sample;
                    queue_try_add(&sharedQueue, &q_word);
                }
            }
        }
    }
}

float Pass(float x){
    return x;
}

float LowPass(float x){
    static float y  = 0.0f;
    y = alpha * x + (1.0f - alpha) * y;
    return y;
}

float HighPass(float x){
    static float y  = 0.0f;
    static float x_prev = 0.0f;
    y = alpha * (y + x - x_prev);
    x_prev = x;
    return y;
}