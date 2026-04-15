#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "tusb.h"
#include "../lib/adc.hpp"
#include "../lib/I2S.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

// Hardware configuration
static const uint8_t ADC_CHANNEL_0    = 0;
static const uint8_t PIN_I2S_Tx_SD    = 0;
static const uint8_t PIN_I2S_Tx_BCLK  = 2;
static const uint8_t PIN_I2S_Tx_WS    = 3;
static const uint    I2S_WS_FRAME_SIZE = 16;
static const float   FS               = 44100.0f;

// Shared state
enum Mode : uint8_t { MODE_ADC = 0, MODE_USB = 1 };

static volatile Mode    g_mode    = MODE_ADC;
static volatile bool    g_playing = false;
static volatile uint8_t g_volume  = 100;

// ADC → core1 reporting (display only).
// Using plain volatile variables instead of the hardware inter-core FIFO so
// that no __sev() is emitted from the audio hot path.  __sev() can wake core1
// from the WFE inside getchar_timeout_us, which immediately triggers a USB CDC
// write; the resulting USB bus activity at every 441-sample interval (~10 ms)
// couples electrical noise back into the ADC input, producing the periodic tap
// sound and the matching centre-line glitch on the function-module display.
static volatile uint16_t g_adc_report_val   = 0;
static volatile bool     g_adc_report_ready = false;

// Audio FIFO - much larger to prevent underruns
#define AUDIO_FIFO_SIZE 8192
static int16_t g_audio_fifo_L[AUDIO_FIFO_SIZE];
static int16_t g_audio_fifo_R[AUDIO_FIFO_SIZE];
static volatile uint32_t g_fifo_read_idx = 0;
static volatile uint32_t g_fifo_write_idx = 0;
static volatile uint32_t g_fifo_count = 0;

static inline bool fifo_push(int16_t L, int16_t R) {
    if (g_fifo_count >= AUDIO_FIFO_SIZE) {
        return false;
    }
    uint32_t next_write = (g_fifo_write_idx + 1) % AUDIO_FIFO_SIZE;
    g_audio_fifo_L[g_fifo_write_idx] = L;
    g_audio_fifo_R[g_fifo_write_idx] = R;
    g_fifo_write_idx = next_write;
    __sync_synchronize();
    g_fifo_count++;
    return true;
}

static inline bool fifo_pop(int16_t* L, int16_t* R) {
    if (g_fifo_count == 0) {
        *L = 0;
        *R = 0;
        return false;
    }
    *L = g_audio_fifo_L[g_fifo_read_idx];
    *R = g_audio_fifo_R[g_fifo_read_idx];
    g_fifo_read_idx = (g_fifo_read_idx + 1) % AUDIO_FIFO_SIZE;
    __sync_synchronize();
    g_fifo_count--;
    return true;
}

static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  32767;
    if (x <= -1.0f) return -32768;
    return (int16_t)(x * 32768.0f);
}

static inline int16_t adc_raw_to_fix(uint16_t raw12) {
    // Convert 12-bit ADC to signed 16-bit centered around 0
    int32_t centered = (int32_t)raw12 - 2048;  // Center around 0 (2048 is ~1.65V)
    // Scale to 16-bit range
    int32_t scaled = (centered * 32767) / 2048;
    return (int16_t)scaled;
}

static inline int16_t apply_vol(int16_t s, uint8_t v) {
    if (v == 0) return 0;
    if (v >= 100) return s;
    return (int16_t)(((int32_t)s * (int32_t)v) / 100);
}

// CORE 0 — real-time I2S loop
static I2S_Tx* g_i2sTx = nullptr;

static void __attribute__((noreturn)) core0_loop() {
    ADC& adc0 = ADC::getActiveChannel();
    uint32_t adc_skip = 0;

    while (true) {
        if (g_mode == MODE_ADC) {
            // ADC path - direct monitoring
            if (adc0.newValue()) {
                uint16_t raw = adc0.rawValue();
                int16_t sample = apply_vol(adc_raw_to_fix(raw), g_volume);
                
                // Send to I2S (mono -> both channels)
                bool ok = false;
                while (!ok) { 
                    ok = g_i2sTx->queue((uint32_t)(uint16_t)sample, (uint32_t)(uint16_t)sample); 
                }
                
                // Throttled ADC report to core1 (for GUI).
                // Only write when core1 has consumed the previous value to
                // avoid clobbering; if core1 is busy the report is simply
                // skipped — an occasional missed display update is fine.
                if (++adc_skip >= 441) {
                    adc_skip = 0;
                    if (!g_adc_report_ready) {
                        g_adc_report_val   = raw;
                        g_adc_report_ready = true;
                    }
                }
            }
        } else {
            // USB audio path - pull from FIFO
            int16_t L, R;
            fifo_pop(&L, &R);
            
            bool ok = false;
            while (!ok) { 
                ok = g_i2sTx->queue((uint32_t)(uint16_t)L, (uint32_t)(uint16_t)R); 
            }
        }
    }
}

// CORE 1 — USB CDC I/O
static char g_cmd[64];
static uint8_t g_cmd_len = 0;

static void usb_send(const char* s) {
    fputs(s, stdout);
    fflush(stdout);
}

static void process_cmd(const char* cmd) {
    if (strncmp(cmd, "MODE ADC", 8) == 0) {
        g_playing = false;
        g_mode = MODE_ADC;
        g_fifo_read_idx = g_fifo_write_idx;
        g_fifo_count = 0;
        usb_send("OK MODE ADC\n");
    } else if (strncmp(cmd, "MODE USB", 8) == 0) {
        g_mode = MODE_USB;
        g_fifo_read_idx = g_fifo_write_idx;
        g_fifo_count = 0;
        usb_send("OK MODE USB\n");
    } else if (strncmp(cmd, "PLAY", 4) == 0) {
        g_playing = true;
        g_fifo_read_idx = g_fifo_write_idx;
        g_fifo_count = 0;
        usb_send("OK PLAY\n");
    } else if (strncmp(cmd, "STOP", 4) == 0) {
        g_playing = false;
        usb_send("OK STOP\n");
    } else if (strncmp(cmd, "VOL ", 4) == 0) {
        int v = atoi(cmd + 4);
        v = (v < 0) ? 0 : (v > 100) ? 100 : v;
        g_volume = (uint8_t)v;
        char buf[20]; 
        snprintf(buf, sizeof(buf), "OK VOL %d\n", v);
        usb_send(buf);
    } else if (strncmp(cmd, "STATUS", 6) == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "STATUS MODE:%s VOL:%d PLAY:%d FIFO:%lu\n",
                 (g_mode == MODE_ADC) ? "ADC" : "USB",
                 (int)g_volume, (int)g_playing, g_fifo_count);
        usb_send(buf);
    }
}

// Binary chunk decoder - FIXED with correct byte order
static void __attribute__((noreturn)) core1_entry() {
    sleep_ms(300);
    usb_send("HELLO FEATHER_INPUT_MODULE v2\n");

    enum State { 
        WAIT_MAGIC1, 
        WAIT_MAGIC2, 
        COUNT_LO, 
        COUNT_HI, 
        L_LO, 
        L_HI, 
        R_LO, 
        R_HI 
    };
    
    State state = WAIT_MAGIC1;
    uint16_t expected_samples = 0;
    uint16_t samples_read = 0;
    uint8_t L_lo = 0, L_hi = 0, R_lo = 0, R_hi = 0;

    while (true) {
        // USB watchdog
        if (g_playing && !tud_cdc_connected()) {
            g_playing = false;
        }

        // Forward ADC reports ONLY in ADC mode, and ONLY to USB (not to audio)
        if (g_mode == MODE_ADC) {
            if (g_adc_report_ready) {
                uint16_t val       = g_adc_report_val;
                g_adc_report_ready = false;          // release for next sample
                char buf[20];
                snprintf(buf, sizeof(buf), "ADC %u\n", (unsigned)val);
                usb_send(buf);
            }
        }

        int c = getchar_timeout_us(100);
        if (c == PICO_ERROR_TIMEOUT) continue;

        uint8_t b = (uint8_t)c;
        bool streaming = (g_mode == MODE_USB && g_playing);

        if (!streaming) {
            // Text command mode
            state = WAIT_MAGIC1;
            if (b == '\n' || b == '\r') {
                if (g_cmd_len > 0) {
                    g_cmd[g_cmd_len] = '\0';
                    process_cmd(g_cmd);
                    g_cmd_len = 0;
                }
            } else if (g_cmd_len < 62) {
                g_cmd[g_cmd_len++] = (char)b;
            }
            continue;
        }

        // Binary chunk decoding - LITTLE-ENDIAN samples
        switch (state) {
            case WAIT_MAGIC1:
                if (b == 0xAA) {
                    state = WAIT_MAGIC2;
                }
                break;
                
            case WAIT_MAGIC2:
                if (b == 0x55) {
                    state = COUNT_LO;
                    samples_read = 0;
                } else if (b == 0xAA) {
                    state = WAIT_MAGIC2;
                } else {
                    state = WAIT_MAGIC1;
                }
                break;
                
            case COUNT_LO:
                expected_samples = b;
                state = COUNT_HI;
                break;
                
            case COUNT_HI:
                expected_samples |= ((uint16_t)b << 8);
                if (expected_samples > 0 && expected_samples <= 1024) {
                    state = L_LO;
                } else {
                    state = WAIT_MAGIC1;
                }
                break;
                
            case L_LO:
                L_lo = b;
                state = L_HI;
                break;
                
            case L_HI:
                L_hi = b;
                state = R_LO;
                break;
                
            case R_LO:
                R_lo = b;
                state = R_HI;
                break;
                
            case R_HI:
                R_hi = b;
                
                // Reconstruct samples - little-endian: low byte first
                int16_t L_val = (int16_t)((uint16_t)L_lo | ((uint16_t)L_hi << 8));
                int16_t R_val = (int16_t)((uint16_t)R_lo | ((uint16_t)R_hi << 8));
                
                // Apply volume
                L_val = apply_vol(L_val, g_volume);
                R_val = apply_vol(R_val, g_volume);
                
                // Push to FIFO
                int timeout = 10000;
                while (!fifo_push(L_val, R_val) && timeout-- > 0) {
                    tight_loop_contents();
                }
                
                samples_read++;
                if (samples_read < expected_samples) {
                    state = L_LO;
                } else {
                    state = WAIT_MAGIC1;
                }
                break;
        }
    }
}

// main()
int main() {
    stdio_init_all();
    gpio_init(29);
    gpio_set_dir(29, GPIO_OUT);
    gpio_put(29,1);

    // ADC init
    ADC::init(FS, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ADC_CHANNEL_0, true);
    ADC::setActiveChannel(ADC_CHANNEL_0);
    ADC::enableIRQ(true);

    // I2S Tx init
    static uint32_t Tx_mem[128 * 8];
    static uint32_t Tx_def[128];
    static I2S_Tx i2sTx(Tx_mem, Tx_def, 8, 128);
    g_i2sTx = &i2sTx;
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, FS, I2S_WS_FRAME_SIZE);

    ADC::run(true);
    i2sTx.enable(true);

    // Launch core1
    multicore_launch_core1(core1_entry);

    // core0 real-time loop
    core0_loop();
}
