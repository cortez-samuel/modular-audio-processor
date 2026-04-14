#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "../lib/I2S.h"
#include "../lib/oled.h"
#include "../lib/adc.hpp"
#include "../lib/fft.hpp"
#include "../lib/PushButton.hpp"
#include "../lib/RotaryEncoder.hpp"
#include "../lib/filters.h"

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
static queue_t sharedQueue;

    // I2S Tx
static const uint8_t PIN_I2S_Tx_SD   = 0;
static const uint8_t PIN_I2S_Tx_BCLK = 2;
static const uint8_t PIN_I2S_Tx_WS   = 3;
static I2S_Tx i2sTx;
static const uint32_t Tx_reservedMemDepth = 128;
static const uint32_t Tx_reservedMemWidth = 8;
static uint32_t Tx_reservedMem[Tx_reservedMemDepth * Tx_reservedMemWidth];
static uint32_t Tx_defaultMem[Tx_reservedMemDepth];

    // I2S Rx
static const uint8_t PIN_I2S_Rx_SD   = 7;
static const uint8_t PIN_I2S_Rx_BCLK = 8;
static const uint8_t PIN_I2S_Rx_WS   = 9;
static I2S_Rx i2sRx;
static const uint32_t Rx_reservedMemDepth = 128;
static uint32_t Rx_reservedMem[Rx_reservedMemDepth * RxPingPong::WIDTH];

    // I2S GENERAL
static const uint  I2S_WS_FRAME_WIDTH = 16;
static const float fs                 = 44100;

    // FILTERS
static float volatile alpha          = 1.0f;
static float alphaMin                = 0.0f;
static float alphaMax                = 1.0f;
static const uint FILTER_BUFFER_DEPTH   = 128;
static float buffer_X[FILTER_BUFFER_DEPTH];
static float buffer_Y[FILTER_BUFFER_DEPTH];
static CyclicBuffer_t<float> cyclicBuffer_X(buffer_X, FILTER_BUFFER_DEPTH);
static CyclicBuffer_t<float> cyclicBuffer_Y(buffer_Y, FILTER_BUFFER_DEPTH);
static FilterInstance_t* currentFilter  = nullptr;

static FilterInstance_t FILTER_PASS {
    .filter_name    = "SRC",
    .filter         = Filters::PASS,
    .x              = &cyclicBuffer_X,
    .y              = &cyclicBuffer_Y,
};
static FilterInstance_t FILTER_LPF {
    .filter_name    = "LPF",
    .filter         = Filters::FirstOrderIIR::LPF,
    .x              = &cyclicBuffer_X,
    .y              = &cyclicBuffer_Y,
};
static FilterInstance_t FILTER_HPF {
    .filter_name    = "HPF",
    .filter         = Filters::FirstOrderIIR::HPF,
    .x              = &cyclicBuffer_X,
    .y              = &cyclicBuffer_Y,
};

enum class Mode { 
    Pass, 
    Lowpass, 
    Highpass, 
    FFT 
};
static Mode mode = Mode::Pass;      // default: SRC / Pass

static const uint64_t PUSH_BUTTON_DEBOUNCE_TIME_us  = 50000;

    // MODE ROTARY ENCODER  — turning cycles mode, pressing resets to SRC
static const uint8_t  PIN_MODE_ENCODER_A             = 12;
static const uint8_t  PIN_MODE_ENCODER_B             = 13;
static const uint8_t  PIN_MODE_ENCODER_SW            = 25;

    // ALPHA ROTARY ENCODER — turning adjusts alpha, pressing resets to 1.00
static const uint8_t  PIN_ALPHA_ENCODER_A            = 1;
static const uint8_t  PIN_ALPHA_ENCODER_B            = 6;
static const uint8_t  PIN_ALPHA_ENCODER_SW           = 20;

static const uint64_t ROTARY_ENCODER_DEBOUNCE_TIME_us   = 10000;
static const uint8_t  ROTARY_ENCODER_MIN_POSITION       = 0;
static const uint8_t  ROTARY_ENCODER_MAX_POSITION       = 100;

    // flags set inside ISR, consumed by core1 main loop
static volatile bool pendingModeChange  = false;    // advance mode (positive turn)
static volatile bool pendingModeReverse = false;    // retreat mode (negative turn)
static volatile bool pendingModeReset   = false;    // reset mode to SRC (SW press)
static volatile bool pendingAlphaReset  = false;    // reset alpha to 1.00 (SW press)
static volatile bool pendingFlashSave   = false;
static volatile bool flashSaveDone      = false;
static volatile float    lastSavedAlpha = -1.0f;   // sentinel: forces first save
static volatile uint32_t lastSavedMode  = 0xFFFFFFFFu;

#define FEATHER_RP2040_FLASH_SIZE   (8u * 1024u * 1024u)   // 8 MB
#define FLASH_MAGIC         0xA55A1234u
#define FLASH_TARGET_OFFSET (FEATHER_RP2040_FLASH_SIZE - FLASH_SECTOR_SIZE)

struct FlashData {
    uint32_t magic;
    float alpha;
    uint32_t mode;      // stored as uint32_t so the struct layout stays simple
};

static void __not_in_flash_func(flashWriteCallback)(void* param) {
    const FlashData* d = reinterpret_cast<const FlashData*>(param);
    static uint8_t pageBuf[FLASH_PAGE_SIZE];
    __builtin_memset(pageBuf, 0xFF, sizeof(pageBuf));
    __builtin_memcpy(pageBuf, d, sizeof(FlashData));
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, pageBuf, FLASH_PAGE_SIZE);
}

// save current alpha and mode to flash
static void saveToFlash(float a, Mode m) {
    static FlashData data;          // static so it stays alive inside the callback
    data.magic = FLASH_MAGIC;
    data.alpha = a;
    data.mode  = static_cast<uint32_t>(m);
    // flash_safe_execute pauses the other core so both aren't executing from flash
    flash_safe_execute(flashWriteCallback, &data, UINT32_MAX);
}

// try to read previously saved data, returns true on success.
static bool loadFromFlash(float& outAlpha, Mode& outMode) {
    const FlashData* d = reinterpret_cast<const FlashData*>(
        XIP_BASE + FLASH_TARGET_OFFSET);
    if (d->magic != FLASH_MAGIC) return false;
    outAlpha = d->alpha;
    outMode  = static_cast<Mode>(d->mode);
    return true;
}

// Advance mode: Pass -> LPF -> HPF -> FFT -> Pass
static inline void cycleMode() {
    switch (mode) {
        case Mode::Pass:     mode = Mode::Lowpass;  currentFilter = &FILTER_LPF;  break;
        case Mode::Lowpass:  mode = Mode::Highpass; currentFilter = &FILTER_HPF;  break;
        case Mode::Highpass: mode = Mode::FFT;      currentFilter = &FILTER_PASS; break;
        case Mode::FFT:      mode = Mode::Pass;     currentFilter = &FILTER_PASS; break;
    }
}

// Retreat mode: Pass -> FFT -> HPF -> LPF -> Pass
static inline void reverseCycleMode() {
    switch (mode) {
        case Mode::Pass:     mode = Mode::FFT;      currentFilter = &FILTER_PASS; break;
        case Mode::FFT:      mode = Mode::Highpass; currentFilter = &FILTER_HPF;  break;
        case Mode::Highpass: mode = Mode::Lowpass;  currentFilter = &FILTER_LPF;  break;
        case Mode::Lowpass:  mode = Mode::Pass;     currentFilter = &FILTER_PASS; break;
    }
}

// Reset mode to default (SRC / Pass)
static inline void resetMode() {
    mode          = Mode::Pass;
    currentFilter = &FILTER_PASS;
}

static const float _E_SCALE = 32768.0f;
static inline float fix_to_float(int16_t x) {
    return static_cast<float>(x) / _E_SCALE;
}
static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  0x7FFF;
    if (x <= -1.0f) return static_cast<int16_t>(0x8000);
    return static_cast<int16_t>(x * 32767.0f);
}
static inline float clamp_f(float lo, float x, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

//  CALLBACKS

// Mode encoder — positive turn: advance mode
static void modeEncPosCallback(
    RotaryEncoder<ROTARY_ENCODER_DEBOUNCE_TIME_us>* /*enc*/,
    RotaryEncoder<ROTARY_ENCODER_DEBOUNCE_TIME_us>::State_t /*next*/)
{
    pendingModeChange = true;
}

// Mode encoder — negative turn: retreat mode
static void modeEncNegCallback(
    RotaryEncoder<ROTARY_ENCODER_DEBOUNCE_TIME_us>* /*enc*/,
    RotaryEncoder<ROTARY_ENCODER_DEBOUNCE_TIME_us>::State_t /*next*/)
{
    pendingModeReverse = true;
}

// Mode encoder switch — press: reset mode to SRC
static void modeEncSwUpCallback(
    PushButton<PUSH_BUTTON_DEBOUNCE_TIME_us>* /*pb*/,
    PushButton<PUSH_BUTTON_DEBOUNCE_TIME_us>::State_t /*next*/)
{
    pendingModeReset = true;
}

// Alpha encoder switch — press: reset alpha to 1.00
static void alphaEncSwUpCallback(
    PushButton<PUSH_BUTTON_DEBOUNCE_TIME_us>* /*pb*/,
    PushButton<PUSH_BUTTON_DEBOUNCE_TIME_us>::State_t /*next*/)
{
    pendingAlphaReset = true;
}

// CORE 1 – OLED DISPLAY + INPUT HANDLING
void core1_entry() {
    flash_safe_execute_core_init();
    multicore_fifo_push_blocking(0);

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

    // load saved state from flash
    {
        float   savedAlpha = 1.0f;          // default: 1.00
        Mode    savedMode  = Mode::Pass;    // default: SRC

        if (loadFromFlash(savedAlpha, savedMode)) {
            savedAlpha = clamp_f(alphaMin, savedAlpha, alphaMax);
        }

        alpha = savedAlpha;
        mode  = savedMode;
        switch (mode) {
            case Mode::Pass:     currentFilter = &FILTER_PASS; break;
            case Mode::Lowpass:  currentFilter = &FILTER_LPF;  break;
            case Mode::Highpass: currentFilter = &FILTER_HPF;  break;
            case Mode::FFT:      currentFilter = &FILTER_PASS; break;
        }
    }

    GPIO_IRQManager::init();

    // Mode rotary encoder — turning cycles mode forward/backward
    RotaryEncoder<ROTARY_ENCODER_DEBOUNCE_TIME_us> modeRotaryEncoder;
    modeRotaryEncoder.setCallback(modeEncPosCallback, false, true);  // positive turn
    modeRotaryEncoder.setCallback(modeEncNegCallback, true,  true);  // negative turn
    modeRotaryEncoder.begin(PIN_MODE_ENCODER_A, PIN_MODE_ENCODER_B);

    // Mode encoder switch — press resets mode to SRC
    PushButton<PUSH_BUTTON_DEBOUNCE_TIME_us> modeEncSwPushButton;
    modeEncSwPushButton.setCallback(modeEncSwUpCallback, true, true);   // onUp
    modeEncSwPushButton.begin(PIN_MODE_ENCODER_SW);

    // Alpha rotary encoder — turning adjusts alpha value
    RotaryEncoder<ROTARY_ENCODER_DEBOUNCE_TIME_us> alphaRotaryEncoder;
    alphaRotaryEncoder.begin(PIN_ALPHA_ENCODER_A, PIN_ALPHA_ENCODER_B);
    alphaRotaryEncoder.setState(static_cast<int>(alpha * 100.0f + 0.5f));

    // Alpha encoder switch — press resets alpha to 1.00
    PushButton<PUSH_BUTTON_DEBOUNCE_TIME_us> alphaEncSwPushButton;
    alphaEncSwPushButton.setCallback(alphaEncSwUpCallback, true, true); // onUp
    alphaEncSwPushButton.begin(PIN_ALPHA_ENCODER_SW);

    // Waveform circular buffer
    static const int CIRCULAR_BUFFER_SIZE = 512;
    uint32_t circularBuffer[CIRCULAR_BUFFER_SIZE] = {0};
    int      bufferWriteIndex = 0;
    uint32_t lastDisplayTime  = 0;
    const uint32_t DISPLAY_INTERVAL_MS = 33;
    static uint32_t lastAutoSaveCheck_ms = 0;

    while(true){
        // service deferred ISR flags
        uint32_t now = time_us_32() / 1000;

        if (pendingModeChange) {
            pendingModeChange = false;
            cycleMode();
        }
        if (pendingModeReverse) {
            pendingModeReverse = false;
            reverseCycleMode();
        }
        if (pendingModeReset) {
            pendingModeReset = false;
            resetMode();
        }
        if (pendingAlphaReset) {
            pendingAlphaReset = false;
            alpha = 1.0f;
            alphaRotaryEncoder.setState(100);   // 1.00 * 100
        }
        if (flashSaveDone) {
            flashSaveDone = false;
        }

        // PERIODIC AUTO-SAVE (every 2 seconds, only when state changed)
        if (now - lastAutoSaveCheck_ms >= 2000) {
            lastAutoSaveCheck_ms = now;
            float    curAlpha = alpha;
            uint32_t curMode  = static_cast<uint32_t>(mode);
            if (curAlpha != lastSavedAlpha || curMode != lastSavedMode) {
                lastSavedAlpha = curAlpha;
                lastSavedMode  = curMode;
                pendingFlashSave = true;
            }
        }

        // alpha encoder -> alpha value (clamped to [0.00, 1.00])
        int encPosition = alphaRotaryEncoder.getState();

        if (encPosition > ROTARY_ENCODER_MAX_POSITION) {
            encPosition = ROTARY_ENCODER_MAX_POSITION;
            alphaRotaryEncoder.setState(ROTARY_ENCODER_MAX_POSITION);
        }
        if (encPosition < ROTARY_ENCODER_MIN_POSITION) {
            encPosition = ROTARY_ENCODER_MIN_POSITION;
            alphaRotaryEncoder.setState(ROTARY_ENCODER_MIN_POSITION);
        }

        alpha = encPosition / 100.0f;

        // Drain inter-core queue
        uint32_t word;
        while (queue_try_remove(&sharedQueue, &word)) {
            circularBuffer[bufferWriteIndex] = word;
            bufferWriteIndex = (bufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
        }
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
                bool     max_gt_64k = fftOut_max > 65535;
                uint32_t scale      = max_gt_64k
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
                    if (x % 4 < 2) oled.drawPixel(x, 32, true);
                }
                int lastY = 32;
                for (int x = 0; x < 128; x++) {
                    int bufIdx = (bufferWriteIndex + x * 4) % CIRCULAR_BUFFER_SIZE;
                    int16_t  s          = (int16_t)(uint16_t)circularBuffer[bufIdx];
                    uint16_t offset_bin = (uint16_t)s ^ 0x8000u;
                    uint8_t  y          = 63 - (uint8_t)(offset_bin >> 10);
                    if (y > 63) y = 63;
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
            oled.setTextSize(1);
            oled.setTextColor(true);
            oled.setCursor(0, 0);
            switch (mode) {
                case Mode::FFT: oled.print("FFT"); break;
                default: oled.print(currentFilter->filter_name); break;
            }
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

// CORE 0 – I/O + FILTER
int main() {
    stdio_init_all();

    // LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Inter-core queue
    queue_init(&sharedQueue, sizeof(uint32_t), 256);

    multicore_launch_core1(core1_entry);
    multicore_fifo_pop_blocking();
    i2sTx.setReservedMem(Tx_reservedMem, Tx_defaultMem, Tx_reservedMemWidth, Tx_reservedMemDepth);
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, fs, I2S_WS_FRAME_WIDTH);
    i2sRx.setReservedMem(Rx_reservedMem, Rx_reservedMemDepth);
    i2sRx.init(PIN_I2S_Rx_BCLK, PIN_I2S_Rx_WS, PIN_I2S_Rx_SD, fs, I2S_WS_FRAME_WIDTH);
    i2sTx.enable(true);
    i2sRx.enable(true);

    // initialize filter + mode
    currentFilter = &FILTER_PASS;
    mode          = Mode::Pass;

    uint32_t downsampleCounter = 0;

    while(true){
        if(pendingFlashSave){
            pendingFlashSave = false;
            // stop pio state machines to remove DREQ
            i2sTx.pause();
            i2sRx.pause();
            float snapAlpha = alpha;
            Mode  snapMode  = mode;
            saveToFlash(snapAlpha, snapMode);
            // resume PIO
            i2sRx.resume();
            i2sTx.resume();
            flashSaveDone = true;
        }

        uint32_t rxBuf[Rx_reservedMemDepth];
        if(i2sRx.readBuffer(rxBuf)){
            bool queuedValid = true;
            for (uint i = 0; i < Rx_reservedMemDepth / 2; i++) {
                // Get stereo sample and mix to mono
                int16_t raw_sample_LC = (int16_t)(uint16_t)rxBuf[2 * i];
                int16_t raw_sample_RC = (int16_t)(uint16_t)rxBuf[2 * i + 1];
                int16_t raw_sample    = (raw_sample_LC >> 1) + (raw_sample_RC >> 1);

                // Apply active filter
                float s_vol        = fix_to_float(raw_sample);
                float filtered_vol = call_filter(currentFilter, s_vol, alpha);
                filtered_vol       = clamp_f(-1.0f, filtered_vol, 1.0f);
                int16_t out_sample = float_to_fix(filtered_vol);

                // Output
                uint32_t tx_word = (uint32_t)(uint16_t)out_sample;
                queuedValid = i2sTx.queue(tx_word, tx_word) & queuedValid;

                // Downsample for display
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