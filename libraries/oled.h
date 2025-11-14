#ifndef OLED_H
#define OLED_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string.h>

// SSD1306 Commands
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

class OLED {
public:
    OLED(spi_inst_t* spi, uint8_t cs_pin, uint8_t dc_pin, uint8_t rst_pin, 
         uint8_t width = 128, uint8_t height = 64);
    
    ~OLED();
    
    // Init
    bool begin(uint32_t spi_speed = 8000000);
    
    // Display
    void display();
    void clearDisplay();
    void invertDisplay(bool invert);
    void setContrast(uint8_t contrast);
    
    // Drawing
    void drawPixel(int16_t x, int16_t y, bool color);
    bool getPixel(int16_t x, int16_t y);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
    void drawCircle(int16_t x0, int16_t y0, int16_t r, bool color);
    void fillCircle(int16_t x0, int16_t y0, int16_t r, bool color);
    
    // Text
    void setCursor(int16_t x, int16_t y);
    void setTextSize(uint8_t size);
    void setTextColor(bool color);
    void write(char c);
    void print(const char* str);
    void println(const char* str);
    
    // Buffer access
    uint8_t* getBuffer() { return buffer; }
    uint16_t getBufferSize() { return buffer_size; }
    
private:
    void spiWrite(uint8_t data);
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void sendDataBurst(uint8_t* data, size_t len);
    
    spi_inst_t* spi_inst;
    uint8_t cs, dc, rst;
    
    uint8_t width, height;
    
    uint8_t* buffer;
    uint16_t buffer_size;
    
    int16_t cursor_x, cursor_y;
    uint8_t text_size;
    bool text_color;
    
    static const uint8_t font5x7[][5];
};

#endif // OLED_H