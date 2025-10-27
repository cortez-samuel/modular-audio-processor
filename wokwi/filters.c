#include "filters.h"

const FilterFunc available_filters[FILTER_COUNT] = {
	low_pass,
	high_pass
};

const char *filter_names[FILTER_COUNT] = {
	"LPF",
	"HPF"
};

float low_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float alpha)
{
	int prev = (index - 1) % 128;
	return ((1 - alpha) * (float)raw[index]) + (alpha * (float)filt[prev]);
}

float high_pass(unsigned int *raw, unsigned int *filt, unsigned int index, float alpha)
{
	int prev = (index - 1) % 128;
	return (alpha * (float)filt[prev]) + (alpha * ((float)raw[index] - raw[prev]));
}
