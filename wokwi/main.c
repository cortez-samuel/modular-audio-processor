#include "pico/stdlib.h"
#include "oled.h"

int main() {
    stdio_init_all();
    oled_init();

    oled_draw_text(0, 0, "abcdefghijklmnopqrstuvwxyz");
    oled_update();


    while (true) {
        tight_loop_contents();
    }
}