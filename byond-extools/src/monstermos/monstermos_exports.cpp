#include "../core/core.h"
#include "monstermos.h"

extern "C" EXPORT const char* init_monstermos(int n_args, const char** args)
{
	if (!Core::initialize())
	{
		return "Extools Init Failed";
	}

	total_num_gases = (int)n_args;

	return enable_monstermos();
}