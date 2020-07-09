#include <stdarg.h>
#include "demo_writer.h"
#include "../core/core.h"
#include "writer.h"

/*void log_world(const char* format, ...) {
	char buffer[256];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 256, format, args);
	std::vector<Value> log_world_args;
	log_world_args.push_back(buffer);
	IncRefCount(log_world_args[0].type, log_world_args[0].value);
	Core::get_proc("/proc/log_world").call(log_world_args);
	va_end(args);
}*/

trvh flush_demo_updates(unsigned int args_len, Value* args, Value src) {
	turf_update_buffer.flush();
	obj_update_buffer.flush();
	mob_update_buffer.flush();
	return Value::Null();
}

SetAppearancePtr oSetAppearance;
void hSetAppearance(trvh atom, int appearance) {
	oSetAppearance(atom, appearance);
	switch (atom.type) {
	case TURF:
	case LIST_TURF_VERBS:
	case LIST_TURF_OVERLAYS:
	case LIST_TURF_UNDERLAYS:
		break;
	case OBJ:
	case LIST_VERBS:
	case LIST_OVERLAYS:
	case LIST_UNDERLAYS:
		obj_update_buffer.mark_dirty(atom.value);
		break;
	case MOB:
	case LIST_MOB_VERBS:
	case LIST_MOB_OVERLAYS:
	case LIST_MOB_UNDERLAYS:
		mob_update_buffer.mark_dirty(atom.value);
		break;
	}
}

SpliceAppearancePtr oSpliceAppearance;
void __fastcall hSpliceAppearance(void* this_, int edx, Appearance* appearance) {
	get_demo_id_flags(appearance->id).appearance_written = false;
	oSpliceAppearance(this_, edx, appearance);
}

SetPixelXPtr oSetPixelX;
SetPixelYPtr oSetPixelY;
SetPixelWPtr oSetPixelW;
SetPixelZPtr oSetPixelZ;

const char* pixel_things[] = { "pixel_x", "pixel_y", "pixel_w", "pixel_z" };

template<SetPixelXPtr &oSetPixel, int num>
void hSetPixel(trvh atom, short pixel_x) {
	if (atom.type == DataType::OBJ) obj_update_buffer.mark_dirty(atom.value);
	else if (atom.type == DataType::MOB) mob_update_buffer.mark_dirty(atom.value);
	oSetPixel(atom, pixel_x);
}

SetMovableDirPtr oSetMovableDir;
void hSetMovableDir(trvh atom, unsigned char dir) {
	switch (atom.type) {
	case DataType::OBJ:
		obj_update_buffer.mark_dirty(atom.value);
		break;
	case DataType::MOB:
		mob_update_buffer.mark_dirty(atom.value);
		break;
	case DataType::TURF:
		turf_update_buffer.mark_dirty(atom.value);
		break;
	}
	oSetMovableDir(atom, dir);
}

SetLocPtr oSetLoc;
void hSetLoc(trvh atom, trvh loc) {
	switch (atom.type) {
	case DataType::OBJ:
		obj_update_buffer.mark_dirty(atom.value);
		break;
	case DataType::MOB:
		mob_update_buffer.mark_dirty(atom.value);
		break;
	}
	oSetLoc(atom, loc);
}


bool enable_demo(const char *out_file, const char *commit_hash)
{
	oSetAppearance = Core::install_hook(SetAppearance, hSetAppearance);
	oSpliceAppearance = Core::install_hook(SpliceAppearance, hSpliceAppearance);
	oSetPixelX = Core::install_hook(SetPixelX, hSetPixel<oSetPixelX, 0>);
	oSetPixelY = Core::install_hook(SetPixelY, hSetPixel<oSetPixelY, 1>);
	oSetPixelW = Core::install_hook(SetPixelW, hSetPixel<oSetPixelW, 2>);
	oSetPixelZ = Core::install_hook(SetPixelZ, hSetPixel<oSetPixelZ, 3>);
	oSetMovableDir = Core::install_hook(SetMovableDir, hSetMovableDir);
	oSetLoc = Core::install_hook(SetLoc, hSetLoc);

	Core::get_proc("/proc/flush_demo_updates").hook(flush_demo_updates);
	demo_file_handle.open(out_file, std::ios::binary | std::ios::trunc);
	// write the header
	demo_file_handle.put(0xCB);
	demo_file_handle.put(0x0);// version number
	demo_file_handle.put(0x0);

	while (*commit_hash != 0) {
		demo_file_handle.put(*commit_hash);
		commit_hash++;
	}
	demo_file_handle.put(0x0);

	write_world_size();

	turf_update_buffer.flush();
	obj_update_buffer.flush();
	mob_update_buffer.flush();

	return true;
}
