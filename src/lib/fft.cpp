// fixed point FFT implementation

#include <stdbool.h>

#include "fft_priv.hpp"

#define abs(n) (n < 0 ? -n : n)

#define FIX_POINT       15
#define FIX_AMT         (1 << FIX_POINT)
#define SAMPLE_AMT_BIT  8
#define SAMPLE_AMT      (1 << SAMPLE_AMT_BIT)

// fixed point multiplication
// shifting before multiplying is insignificant loss to avoid overflow
static fi16_32 fix_mul(fi16_32 a, fi16_32 b)
{
	fi16_32 m = (a >> (FIX_POINT / 2)) * (b >> (FIX_POINT / 2));
	return m >> (FIX_POINT & 1);
}

static Complex fix_cplx_mul(Complex a, Complex b)
{
	Complex c;
	c.r = fix_mul(a.r, b.r) - fix_mul(a.i, b.i);
	c.i = fix_mul(a.r, b.i) + fix_mul(a.i, b.r);
	return c;
}

static fi16_32 fix_cplx_abs(Complex a)
{
	fi16_32 rabs = abs(a.r);
	fi16_32 iabs = abs(a.i);

	fi16_32 max, min;
	if (rabs > iabs) {
		max = rabs;
		min = iabs;
	} else {
		max = iabs;
		min = rabs;
	}

	return max + ((min >> 3) * 3);
}

static void fft_fixed_recur(fi16 *x, Complex *y, uint16_t n, uint16_t s, bool do_shift) {
	if (n == 1) {
		y->r = *x;
		y->i = 0;
		return;
	}

	uint16_t n2 = n/2;
	fft_fixed_recur(x,     y,      n2, s*2, !do_shift);
	fft_fixed_recur(x + s, y + n2, n2, s*2, !do_shift);
	for (uint16_t k = 0; k < n2; k++) {
		Complex p = y[k];
		Complex c;
		// twiddle values, e^(2*pi*k/n)
		c.r = FIX_COS[k * s];
		c.i = -FIX_COS[((k * s) + ((SAMPLE_AMT/4)*3)) & (SAMPLE_AMT - 1)];
		Complex q = fix_cplx_mul(c, y[k+n2]);

		y[k].r    = (p.r + q.r) >> do_shift;
		y[k].i    = (p.i + q.i) >> do_shift;
		y[k+n2].r = (p.r - q.r) >> do_shift;
		y[k+n2].i = (p.i - q.i) >> do_shift;
	}
}

// assumes x& y are SAMPLE_AMT size
void fft_fixed(fi16 *x, fi16_32 *y) {
	Complex fft[SAMPLE_AMT];

	fft_fixed_recur(x, fft, SAMPLE_AMT, 1, true);

	for (int i = 0; i < SAMPLE_AMT; i++) {
		y[i] = fix_cplx_abs(fft[i]);
	}
}
