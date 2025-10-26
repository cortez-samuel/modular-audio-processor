#ifndef OLED_H
#define OLED_H

#include "hardware/i2c.h"
#include "pico/stdlib.h"

// SH1107 I2C configuration
#define SH1107_ADDR 0x3C
#define I2C_PORT i2c0
#define SDA_PIN 20
#define SCL_PIN 21

#define OLED_WIDTH 128
#define OLED_HEIGHT 128

// Public functions
void oled_init(void);
void oled_fill(uint8_t color);
void oled_send_command(uint8_t cmd);
void oled_send_data(uint8_t *data, size_t len);

// Drawing
void oled_set_pixel(uint8_t x, uint8_t y, uint8_t value);
void oled_draw_char(uint8_t x, uint8_t y, char c);
void oled_draw_text(uint8_t x, uint8_t y, const char *str);
void oled_update(void);

#endif