#include "filters.h"

const FilterFunc FILTERS_AVAILABLE[FILTER_COUNT] = {
	filter_low_pass,
	filter_high_pass
};

const char *FILTERS_NAMES[FILTER_COUNT] = {
	"LPF",
	"HPF"
};

float filter_low_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float param)
{
	int prev = (index - 1) % 128;
	return ((1 - param) * (float)raw[index]) + (param * (float)filt[prev]);
}

float filter_high_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float param)
{
	int prev = (index - 1) % 128;
	return (param * (float)filt[prev]) + (param * ((float)raw[index] - raw[prev]));
}
