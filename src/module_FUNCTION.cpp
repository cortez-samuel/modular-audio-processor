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

// ---------------------------------------------------------------------------
// Fixed-point convention  (_E = 15)
//
//   int16_t sample:  1 sign bit, 15 fractional bits
//   Range  : [-1.0,  +0.999969...]   (i.e. 0x8000 -> -1.0, 0x7FFF -> ~+1.0)
//   LSB    :  1/32768  ~  0.0000305
//
// Conversion helpers
//   fix -> float : s_vol = (float)sample / 32768.0f
//   float -> fix : sample = (int16_t)(s_vol * 32767.0f)   [with saturation]
//
// Storage convention
//   Samples are kept as int16_t.  When stuffed into a uint32_t (queue,
//   circularBuffer) the bit pattern is preserved by casting via uint16_t:
//       store : uint32_t word = (uint32_t)(uint16_t)sample;
//       load  : int16_t  s    = (int16_t)(uint16_t)word;
//
// I2S Tx convention
//   i2sTx.queue(LC, RC) expects the 16-bit word in the lower 16 bits of the
//   uint32_t argument.  queue() left-shifts by (32 - WS_frame_size) = 16
//   before writing to the PIO FIFO, placing the signed 16-bit sample in the
//   correct position for the DAC.
//
// I2S Rx convention
//   The PIO ISR is configured for left-shift with autopush at WS_frame_size=16
//   bits, so the captured sample arrives in the lower 16 bits of the 32-bit
//   FIFO word.  Extract with: int16_t s = (int16_t)(uint16_t)rxBuf[i];
// ---------------------------------------------------------------------------

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
// Queue element is uint32_t; we store the int16_t bit-pattern in the low 16 bits.
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
// Filter functions operate on normalised signed voltage in [-1.0, +1.0).
static float (*currentFilter)(float) = nullptr;
static float filterOutput            = 0.0f;
static float Pass(float x);
static float LowPass(float x);
static float HighPass(float x);

enum class Mode { Pass, Lowpass, Highpass, FFT };
static Mode mode = Mode::Pass;

// ---------------------------------------------------------------------------
// Fixed-point helpers
// ---------------------------------------------------------------------------
static const float _E_SCALE = 32768.0f;    // 2^15

// int16_t fixed-point  ->  float in [-1.0, +0.999969...]
static inline float fix_to_float(int16_t x) {
    return (float)x / _E_SCALE;
}

// float in [-1, 1)  ->  int16_t  (saturating)
static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  0x7FFF;
    if (x <= -1.0f) return (int16_t)0x8000;
    return (int16_t)(x * 32767.0f);
}

// General float clamp
static inline float clamp_f(float lo, float x, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////


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
                // -------------------------------------------------------
                // FFT display
                // -------------------------------------------------------
                // Re-interpret the stored uint32_t words as signed int16_t
                // samples (bit-pattern in low 16 bits).
                fi16 fftIn[256];
                for (int i = 0; i < 256; i++) {
                    fftIn[i] = (fi16)(int16_t)(uint16_t)circularBuffer[i];
                }

                // Light DC removal – with a properly biased input the mean
                // should already be ~0, but this costs little.
                int32_t mean = 0;
                for (int i = 0; i < 256; i++) { mean += fftIn[i]; }
                mean >>= 8; // divide by 256
                for (int i = 0; i < 256; i++) { fftIn[i] -= (fi16)mean; }

                fi16_32 fftOut[256];
                fft_fixed(fftIn, fftOut);

                // Normalise to screen height using left (positive-frequency) half only.
                fi16_32 fftOut_max = 1;
                for (int i = 0; i < 128; i++) {
                    if (fftOut[i] > fftOut_max) fftOut_max = fftOut[i];
                }

                // Integer scaling to avoid floats
                bool       max_gt_64k  = fftOut_max > 65535;
                uint32_t   scale       = max_gt_64k
                                             ? (uint32_t)(fftOut_max / 65535 + 1)
                                             : (uint32_t)(65535 / fftOut_max);

                for (int x = 0; x < 128; x++) {
                    uint16_t dp = (uint16_t)(max_gt_64k
                                                 ? (fftOut[x] / scale)
                                                 : (fftOut[x] * scale));
                    // Map 16-bit magnitude to screen row [0..63] (top = loud)
                    uint8_t y = 63 - (uint8_t)(dp >> 10);
                    for (int row = 63; row > (int)y; row--) {
                        oled.drawPixel(x, row, true);
                    }
                }

            } else {
                // -------------------------------------------------------
                // Waveform display
                // -------------------------------------------------------

                // Centre dashed line
                for (int x = 0; x < 128; x++) {
                    if (x % 4 < 2) oled.drawPixel(x, 32, true);
                }

                int lastY = 32;
                for (int x = 0; x < 128; x++) {
                    int bufIdx = (bufferWriteIndex
                                  - CIRCULAR_BUFFER_SIZE
                                  + (x * CIRCULAR_BUFFER_SIZE) / 128
                                  + CIRCULAR_BUFFER_SIZE) % CIRCULAR_BUFFER_SIZE;

                    // Recover signed sample from stored bit-pattern.
                    int16_t s = (int16_t)(uint16_t)circularBuffer[bufIdx];

                    // Map signed int16_t [-32768, 32767] to screen row [0, 63].
                    //   XOR with 0x8000 converts two's-complement to offset-binary:
                    //     0x8000 (-32768) -> 0x0000 -> row 63  (bottom, most negative)
                    //     0x0000 (     0) -> 0x8000 -> row 31  (centre)
                    //     0x7FFF (+32767) -> 0xFFFF -> row  0  (top,    most positive)
                    uint16_t offset_bin = (uint16_t)s ^ 0x8000u;
                    uint8_t  y          = 63 - (uint8_t)(offset_bin >> 10);
                    if (y > 63) y = 63; // clamp for safety

                    // Connect to previous point with a vertical line segment
                    if (x > 0) {
                        int diff = (int)y - lastY;
                        if (diff > 0) {
                            for (int dy = lastY; dy <= (int)y; dy++)
                                oled.drawPixel(x, dy, true);
                        } else if (diff < 0) {
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

    // Misc GPIO
    gpio_init(6);
    gpio_set_dir(6, GPIO_IN);
    gpio_pull_down(6);

    currentFilter = Pass;
    mode          = Mode::Pass;

    ADC::run(true);

    // -----------------------------------------------------------------------
    // Main loop
    // -----------------------------------------------------------------------
    uint32_t downsampleCounter = 0;

    while (true) {
        // ---- Mode switching (debounced button) ----
        static bool     lastButtonState  = false;
        static uint32_t lastDebounceTime = 0;
        const uint32_t  DEBOUNCE_MS      = 100;

        bool     buttonState = gpio_get(CHANGE_MODE);
        uint32_t now         = time_us_32() / 1000;

        if (buttonState && !lastButtonState && (now - lastDebounceTime > DEBOUNCE_MS)) {
            lastDebounceTime = now;
            switch (mode) {
                case Mode::Pass:
                    mode = Mode::Lowpass;  currentFilter = LowPass; break;
                case Mode::Lowpass:
                    mode = Mode::Highpass; currentFilter = HighPass; break;
                case Mode::Highpass:
                    mode = Mode::FFT;      currentFilter = Pass;     break;
                case Mode::FFT:
                    mode = Mode::Pass;     currentFilter = Pass;     break;
            }
        }
        lastButtonState = buttonState;

        // ---- Alpha potentiometer (smoothed) ----
        if (adc0.newValue()) {
            float target = clamp_f(alphaMin, adc0.trueValue() / 3.3f, alphaMax);
            const float smooth = 0.05f;
            alpha = target * smooth + alpha * (1.0f - smooth);
            if (alpha <= alphaMin + 0.005f) alpha = 0.0f;
        }

        // ---- Audio processing ----
        uint32_t rxBuf[reservedMemDepth];

        if (i2sRx.readBuffer(rxBuf)) {
            for (int i = 0; i < (int)reservedMemDepth; i++) {

                // --- Extract signed 16-bit sample from I2S Rx FIFO word ---
                // The PIO ISR shifts left (LSB-first into ISR), autopush at 16
                // bits => the 16-bit sample occupies bits [15:0] of the 32-bit
                // FIFO word.  Casting via uint16_t preserves the bit-pattern
                // before sign-extending to int16_t.
                int16_t raw_sample = (int16_t)(uint16_t)rxBuf[i];

                // --- Convert to normalised signed float for filter ---
                float s_vol = fix_to_float(raw_sample);   // in [-1.0, +1.0)

                // --- Apply selected filter ---
                float filtered_vol = currentFilter(s_vol);

                // --- Saturate and convert back to signed fixed-point ---
                filtered_vol   = clamp_f(-1.0f, filtered_vol, 1.0f);
                int16_t out_sample = float_to_fix(filtered_vol);

                // --- Send to DAC via I2S Tx ---
                // Preserve sign bits: cast int16_t -> uint16_t -> uint32_t.
                // queue() will left-shift by 16, placing the sample in the MSBs
                // of the 32-bit FIFO word as the DAC expects.
                uint32_t tx_word = (uint32_t)(uint16_t)out_sample;
                i2sTx.queue(tx_word, tx_word);

                // --- Downsample for display queue ---
                downsampleCounter++;
                if (downsampleCounter >= DOWNSAMPLE_FACTOR) {
                    downsampleCounter = 0;
                    // Store the int16_t bit-pattern in a uint32_t for the queue.
                    uint32_t q_word = (uint32_t)(uint16_t)out_sample;
                    queue_try_add(&sharedQueue, &q_word);
                }
            }
        }
    }
}


// ---------------------------------------------------------------------------
// Filter implementations
//
// All filters receive and return normalised signed voltage in [-1.0, +1.0).
//
// LPF:  y[n] = alpha*x[n] + (1-alpha)*y[n-1]
//   alpha -> 1 : passes everything (flat)
//   alpha -> 0 : heavily smoothed (very low cutoff)
//
// HPF:  y[n] = alpha * (y[n-1] + x[n] - x[n-1])
//   alpha -> 1 : passes everything (flat)
//   alpha -> 0 : blocks everything
//
// Both filters are already centred at 0 with signed input, so there is no
// DC-offset issue that required special handling with unsigned data.
// ---------------------------------------------------------------------------

float Pass(float x) {
    return x;
}

float LowPass(float x) {
    static float y  = 0.0f;
    y = alpha * x + (1.0f - alpha) * y;
    return y;
}

float HighPass(float x) {
    static float y  = 0.0f;
    static float x_prev = 0.0f;
    y = alpha * (y + x - x_prev);
    x_prev = x;
    return y;
}