#include "filters.h"


namespace Filters {
	float PASS(CyclicBuffer_t<float>* x, CyclicBuffer_t<float>* y, float x_n, float param) {
		return x_n;
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