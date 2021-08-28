#include "maptick.h"
#include "../core/core.h"
#include <chrono>
#include "../datum_socket/datum_socket.h"

SendMapsPtr oSendMaps;

void hSendMaps()
{
	auto start = std::chrono::high_resolution_clock::now();
	oSendMaps();
	auto end = std::chrono::high_resolution_clock::now();
	Value::Global().set("internal_tick_usage", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 100000.0f);
}

bool enable_maptick()
{
	oSendMaps = Core::install_hook(SendMaps, hSendMaps);
	return oSendMaps;
}
