#include "../lib/filters.h"
#include "../lib/mode.h"

#include "pico/stdlib.h"
#include <cstdio>

float LPF_func(float x, float alpha) {
    static float y = 0.0f;
    y = alpha * x + (1.0f - alpha) * y;
    return y;
}

int main() {
    stdio_init_all();

    static const uint N = 128;
    float buffX[N];
    CyclicBuffer_t<float> x(buffX, N);
    float buffY[N];
    CyclicBuffer_t<float> y(buffY, N);

    for (uint i = 0; i < N; i++) {
        x.appendHead(0);
        y.appendHead(0);
    }

    sleep_ms(5000);

    for (uint i = 0; i < N; i++) {
        float y_i = call_filter(Mode::Lowpass, &x, &y, 0.15f, i);
        float expected_y_i = LPF_func(i, 0.15f);
        printf("y_%u = %f\texpected: %f\n", i, y_i, expected_y_i);
    }

    while (1) {
        tight_loop_contents();
    }
}