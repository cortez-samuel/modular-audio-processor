#include "pico/stdlib.h"
#include "oled.h"

int main() {
    stdio_init_all();
    oled_init();
    oled_fill(0);

    oled_draw_text(0, 0, "Hello WORLD!");

    while (true) {
        tight_loop_contents();
    }
}
