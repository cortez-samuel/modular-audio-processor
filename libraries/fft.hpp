#ifndef fft_hpp_INCLUDED
#define fft_hpp_INCLUDED

#include <stdint.h>

// signed fixed-point 16 bit
// raw fi16 only for input data, all intermediaries need i32
typedef int16_t fi16;
typedef int32_t fi16_32;

typedef struct Complex {
	fi16_32 r;
	fi16_32 i;
} Complex;

void fft_fixed(fi16 *x, fi16_32 *y);

#endif // fft_hpp_INCLUDED
