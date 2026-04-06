#ifndef filters_h_INCLUDED
#define filters_h_INCLUDED

#include "cyclicBuffer.hpp"


typedef float (*FilterFunc_t)(CyclicBuffer_t<float>*, CyclicBuffer_t<float>*, float, float);
struct FilterInstance_t {
    const char* filter_name;
    FilterFunc_t filter;
        // y[n] = filter(x, y)
    CyclicBuffer_t<float> *x    = nullptr;
    CyclicBuffer_t<float> *y    = nullptr;
};
float call_filter(FilterInstance_t inst, float param, float x_n);


namespace Filters {
  namespace FirstOrderIIR{
    float LPF(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);
    float HPF(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param);
  }
}

#endif // filters_h_INCLUDED
