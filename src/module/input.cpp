#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "../lib/adc.hpp"
#include "../lib/I2S.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
//  Hardware configuration
// ─────────────────────────────────────────────────────────────────────────────
static const uint8_t ADC_CHANNEL_0    = 0;
static const uint8_t PIN_I2S_Tx_SD    = 0;
static const uint8_t PIN_I2S_Tx_BCLK  = 2;
static const uint8_t PIN_I2S_Tx_WS    = 3;
static const uint    I2S_WS_FRAME_SIZE = 16;
static const float   FS               = 44100.0f;

// ─────────────────────────────────────────────────────────────────────────────
//  Shared state — written by core1, read by core0
//  uint8_t/bool reads are atomic on Cortex-M0+
// ─────────────────────────────────────────────────────────────────────────────
enum Mode : uint8_t { MODE_ADC = 0, MODE_USB = 1 };

static volatile Mode    g_mode    = MODE_ADC;
static volatile bool    g_playing = false;
static volatile uint8_t g_volume  = 100;  // 0–100

// ─────────────────────────────────────────────────────────────────────────────
//  Fixed-point / math helpers
// ─────────────────────────────────────────────────────────────────────────────
static inline int16_t float_to_fix(float x) {
    if (x >=  1.0f) return  0x7FFF;
    if (x <= -1.0f) return (int16_t)0x8000;
    return (int16_t)(x * 32768.0f);
}
static inline int16_t adc_raw_to_fix(uint16_t raw12) {
    float s = (raw12 * (3.3f / 4096.0f) - 1.65f) / 1.65f;
    return float_to_fix(s);
}
static inline int16_t apply_vol(int16_t s, uint8_t v) {
    if (v == 0)   return 0;
    if (v == 100) return s;
    return (int16_t)(((int32_t)s * v) / 100);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SIO FIFO word layout
//   Audio word : bit31=0, bits[31:16]=Left int16, bits[15:0]=Right int16
//   ADC report : bit31=1, bits[11:0]=raw 12-bit ADC value
// ─────────────────────────────────────────────────────────────────────────────
static inline uint32_t pack_audio(int16_t L, int16_t R) {
    return ((uint32_t)(uint16_t)L << 16) | (uint16_t)R;
}
static inline uint32_t pack_adc_report(uint16_t raw) {
    return 0x80000000u | (raw & 0xFFFu);
}
static inline bool word_is_adc_report(uint32_t w) { return w & 0x80000000u; }

// ─────────────────────────────────────────────────────────────────────────────
//  CORE 0 — real-time I2S loop
// ─────────────────────────────────────────────────────────────────────────────
static I2S_Tx* g_i2sTx = nullptr;

static void __attribute__((noreturn)) core0_loop() {
    ADC& adc0 = ADC::getActiveChannel();
    uint32_t adc_skip = 0;

    while (true) {
        if (g_mode == MODE_ADC) {
            // ── ADC path ────────────────────────────────────────────────
            if (adc0.newValue()) {
                uint16_t raw    = adc0.rawValue();
                int16_t  sample = apply_vol(adc_raw_to_fix(raw), g_volume);
                uint32_t word   = (uint32_t)(uint16_t)sample;

                // Blocking push to I2S — this IS the real-time deadline
                bool ok = false;
                while (!ok) { ok = g_i2sTx->queue(word, word); }

                // Throttled ADC report to core1 (~100 Hz)
                if (++adc_skip >= 441) {
                    adc_skip = 0;
                    // Non-blocking: if FIFO full we skip this report
                    multicore_fifo_push_timeout_us(pack_adc_report(raw), 0);
                }
            }
        } else {
            // ── USB audio path ───────────────────────────────────────────
            // Drain SIO FIFO words pushed by core1
            if (multicore_fifo_rvalid()) {
                uint32_t w = multicore_fifo_pop_blocking();
                if (!word_is_adc_report(w)) {
                    int16_t L = (int16_t)(w >> 16);
                    int16_t R = (int16_t)(w & 0xFFFF);
                    // Non-blocking I2S queue — drop if full to avoid stalling
                    g_i2sTx->queue((uint32_t)(uint16_t)L, (uint32_t)(uint16_t)R);
                }
            } else {
                // Underrun: push silence to keep I2S clock alive
                g_i2sTx->queue(0, 0);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  CORE 1 — USB CDC I/O, command parser, binary chunk decoder
// ─────────────────────────────────────────────────────────────────────────────

// Text command buffer
static char     g_cmd[64];
static uint8_t  g_cmd_len = 0;

static void usb_send(const char* s) {
    fputs(s, stdout);
    fflush(stdout);
}

static void process_cmd(const char* cmd) {
    if (strncmp(cmd, "MODE ADC", 8) == 0) {
        g_playing = false;
        g_mode    = MODE_ADC;
        usb_send("OK MODE ADC\n");
    } else if (strncmp(cmd, "MODE USB", 8) == 0) {
        g_mode = MODE_USB;
        usb_send("OK MODE USB\n");
    } else if (strncmp(cmd, "PLAY", 4) == 0) {
        g_playing = true;
        usb_send("OK PLAY\n");
    } else if (strncmp(cmd, "STOP", 4) == 0) {
        g_playing = false;
        usb_send("OK STOP\n");
    } else if (strncmp(cmd, "VOL ", 4) == 0) {
        int v = atoi(cmd + 4);
        v = (v < 0) ? 0 : (v > 100) ? 100 : v;
        g_volume = (uint8_t)v;
        char buf[20]; snprintf(buf, sizeof(buf), "OK VOL %d\n", v);
        usb_send(buf);
    } else if (strncmp(cmd, "STATUS", 6) == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "STATUS MODE:%s VOL:%d PLAY:%d\n",
                 (g_mode == MODE_ADC) ? "ADC" : "USB",
                 (int)g_volume, (int)g_playing);
        usb_send(buf);
    }
    // Unknown commands silently ignored
}

// Binary chunk decoder state machine
static void __attribute__((noreturn)) core1_entry() {
    sleep_ms(300);  // Let core0 bring up I2S first
    usb_send("HELLO FEATHER_INPUT_MODULE v2\n");

    // State machine variables
    uint8_t  sm     = 0;   // 0=m0,1=m1,2=clo,3=chi,4=Llo,5=Lhi,6=Rlo,7=Rhi
    uint16_t count  = 0;
    uint8_t  L_lo   = 0;
    int16_t  L_val  = 0;
    uint8_t  R_lo   = 0;

    while (true) {
        // ── Forward ADC reports (core0→core1) to USB PC ─────────────────
        // Only in ADC mode; non-blocking check
        if (g_mode == MODE_ADC && multicore_fifo_rvalid()) {
            uint32_t w = multicore_fifo_pop_blocking();
            if (word_is_adc_report(w)) {
                char buf[20];
                snprintf(buf, sizeof(buf), "ADC %u\n", (unsigned)(w & 0xFFFu));
                usb_send(buf);
            }
        }

        // ── Read one byte from USB CDC ───────────────────────────────────
        int c = getchar_timeout_us(50);  // 50 µs timeout keeps loop fast
        if (c == PICO_ERROR_TIMEOUT) continue;

        uint8_t b = (uint8_t)c;
        bool streaming = (g_mode == MODE_USB && g_playing);

        if (!streaming) {
            // ── Text command mode ────────────────────────────────────────
            sm = 0;  // Reset binary SM on any non-streaming byte
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

        // ── Binary chunk state machine ────────────────────────────────────
        // Runs only when MODE_USB + PLAY
        switch (sm) {
            case 0:  sm = (b == 0xAA) ? 1 : 0;                               break;
            case 1:  sm = (b == 0x55) ? 2 : (b == 0xAA ? 1 : 0);            break;
            case 2:  count  = b;                       sm = 3;               break;
            case 3:  count |= ((uint16_t)b << 8);
                     sm = (count > 0) ? 4 : 0;                               break;
            case 4:  L_lo = b;                         sm = 5;               break;
            case 5:  L_val = (int16_t)((uint16_t)L_lo | ((uint16_t)b << 8));
                     sm = 6;                                                  break;
            case 6:  R_lo = b;                         sm = 7;               break;
            case 7: {
                int16_t R_val = (int16_t)((uint16_t)R_lo | ((uint16_t)b << 8));
                L_val = apply_vol(L_val, g_volume);
                R_val = apply_vol(R_val, g_volume);
                // Push to core0 — non-blocking; drop on overflow
                multicore_fifo_push_timeout_us(pack_audio(L_val, R_val), 0);
                sm = (--count > 0) ? 4 : 0;
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  main() — core0
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    stdio_init_all();

    // ADC init
    ADC::init(FS, true, false, 1, defaultADCRIQHandler);
    ADC::enableChannel(ADC_CHANNEL_0, true);
    ADC::setActiveChannel(ADC_CHANNEL_0);
    ADC::enableIRQ(true);

    // I2S Tx init — static storage avoids stack issues
    static uint32_t Tx_mem[128 * 8];
    static uint32_t Tx_def[128];
    static I2S_Tx   i2sTx(Tx_mem, Tx_def, 8, 128);
    g_i2sTx = &i2sTx;
    i2sTx.init(PIN_I2S_Tx_BCLK, PIN_I2S_Tx_WS, PIN_I2S_Tx_SD, FS, I2S_WS_FRAME_SIZE);

    ADC::run(true);
    i2sTx.enable(true);

    // Launch core1 (USB / commands) — must happen after I2S is up
    multicore_launch_core1(core1_entry);

    // core0 real-time loop — never returns
    core0_loop();
}
