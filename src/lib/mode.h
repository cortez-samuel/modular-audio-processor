#ifndef mode_h_INCLUDED
#define mode_h_INCLUDED

#include "cyclicBuffer.hpp"
#include "filters.h"

typedef struct {
    const char* name;
    FilterFunc_t filter;
} Mode_t;

enum class Mode : uint32_t { Pass = 0, Lowpass, Highpass, FFT };

static const uint32_t MODES_NUM = 5;
static Mode_t MODES[MODES_NUM] = {
	{ .name = "SRC",   .filter = Filters::PASS,               },
	{ .name = "LPF",   .filter = Filters::FirstOrderIIR::LPF, },
	{ .name = "HPF",   .filter = Filters::FirstOrderIIR::HPF, },
	{ .name = "FFT",   .filter = Filters::PASS,               },
};

static inline Mode u32_to_mode(uint32_t i) { return static_cast<Mode>(i); }
static inline uint32_t mode_to_u32(Mode m) { return static_cast<uint32_t>(m); }
static inline Mode mode_cycle(Mode m) {
	return u32_to_mode((mode_to_u32(m) + 1) % MODES_NUM);
}
static inline Mode mode_cycle_rev(Mode m) {
	return u32_to_mode((mode_to_u32(m) + (MODES_NUM - 1)) % MODES_NUM);
}
static inline Mode mode_default() { return Mode::Pass; }

float call_filter(Mode m, CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param) {
	Mode_t inst = MODES[mode_to_u32(m)];
	float y_n = inst.filter(x, y, x_n, param);

	x->appendHead(x_n);
	y->appendHead(y_n);

	return y_n;
}

#endif // mode_h_INCLUDED
