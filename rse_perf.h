#pragma once

#include "rse_ds.h"

namespace rse {

	namespace perf {

        typedef void (*PerfFunc) (void*);

        static double TimeIt(PerfFunc Perf_Func, void* func_args, int iterations = 1) {

            TickTock t = Tick();
            for (int i = 0; i < iterations; i++) Perf_Func(func_args);
            double result = Tock(t);
            return result;
        }
	}
}