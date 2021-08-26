#include "core.h"

extern "C" EXPORT const char* core_initialize(int n_args, const char** args)
{
	if (!Core::initialize())
	{
		Core::Alert("Core init failed!");
		return Core::FAIL;
	}
	//optimizer_initialize();
#ifdef _WIN32 // i ain't fixing this awful Linux situation anytime soon
	//extended_profiling_initialize();
#endif
	return Core::SUCCESS;
}

extern "C" EXPORT const char* cleanup(int n_args, const char** args)
{
	Core::cleanup();
	return Core::SUCCESS;
}
