#include "pico/stdlib.h"
#include "oled.h"

int main() {
    stdio_init_all();
    oled_init();

    //excuse this, i still haven't figured out why the display cuts off
    //wont be using these anyway, need to implement SPI instead
    //wokwi only supports I2C with this display
    oled_draw_text(0, 3, "Pass             Low-");
    oled_draw_border(); 
    oled_draw_text(0, 117, "ff: 1500Hz       Cuto");
    oled_update();


    while (true) {
        tight_loop_contents();
    }
}