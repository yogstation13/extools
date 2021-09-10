#include "../core/core.h"
#include "joao.h"

extern "C" EXPORT const char* init_joao(int n_args, const char** args)
{
	if (!Core::initialize())
	{
		return "Extools Init Failed";
	}
	return enable_joao();
}