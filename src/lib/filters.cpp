#include "filters.h"


namespace Filters {
	float PASS(CyclicBuffer_t<float>* x, CyclicBuffer_t<float>* y, float x_n, float param) {
		return x_n;
	}

	float GAIN(CyclicBuffer_t<float>* x, CyclicBuffer_t<float>* y, float x_n, float param) {
		// quadratic scaling: x^2 + .5x + .5
		// maps 0 -> 0.5, 0.5 -> 1, 1 -> 2
		float r = (param * param) + (0.5f * param) + (0.5f);
		float ret = x_n * r;
		return (ret > 1) ? 1 : ret;
	}

	namespace FirstOrderIIR {
		float LPF(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param) {
			float y_n1 = y->getHead();
			return param * x_n + (1 - param) * y_n1;
		}

		float HPF(CyclicBuffer_t<float> *x, CyclicBuffer_t<float> *y, float x_n, float param) {
			float x_n1 = x->getHead();
			float y_0 = y->getHead();
			return (param * y_0) + (param * (x_n - x_n1));
		}
	}
}