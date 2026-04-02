#ifndef filters_h_INCLUDED
#define filters_h_INCLUDED

#define FILTER_COUNT 3

typedef float (*FilterFunc)(unsigned int*, unsigned int*, unsigned int, float);

float filter_low_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float param);
float filter_high_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float param);

extern const FilterFunc FILTERS_AVAILABLE[FILTER_COUNT];
extern const char* FILTERS_NAMES[FILTER_COUNT];

#endif // filters_h_INCLUDED
