#include "../core/core.h"
#include "demo_writer.h"

extern "C" EXPORT const char* demo_initialize(int n_args, const char** args)
{
	if (n_args < 2) return Core::FAIL;
	if (!(Core::initialize() && enable_demo(args[0], args[1])))
	{
		return Core::FAIL;
	}
	return Core::SUCCESS;
}
