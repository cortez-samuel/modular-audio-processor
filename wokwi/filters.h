#ifndef filters_h_INCLUDED
#define filters_h_INCLUDED

#define FILTER_COUNT 2

typedef float (*FilterFunc)(unsigned int*, unsigned int*, unsigned int, float);

float low_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float alpha);
float high_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float alpha);

extern const FilterFunc available_filters[FILTER_COUNT];
extern const char* filter_names[FILTER_COUNT];

#endif // filters_h_INCLUDED
