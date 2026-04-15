#ifndef filters_h_INCLUDED
#define filters_h_INCLUDED

#include "cyclicBuffer.hpp"

typedef float (*FilterFunc_t)(CyclicBuffer_t<float>*, CyclicBuffer_t<float>*, float, float);

namespace Filters {
	float PASS(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);
	float GAIN(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);
	float QUANTIZE(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);

	float LPF(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);
	float HPF(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);
}

#endif // filters_h_INCLUDED
