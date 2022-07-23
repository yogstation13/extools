#include "writer.h"
#include <vector>
#include <unordered_set>
#include "animation_writer.h"

// Since clients don't change often I'm okay with using slower GetVariable calls here
// Especially since the client struct is ~1KB, and likely very volatile - while it's trivial
// to reverse engineer these structs, I'd have to constantly update it and that's no fun.
// One thing to keep in mind is that string IDs for builtin vars are hardcoded.

struct DemoPlayerInfo {
	Value last_mob;
	Value last_eye;
	unsigned char last_view_width;
	unsigned char last_view_height;
	union {
		struct {
			unsigned char dirty_login : 1;
			unsigned char dirty_logout : 1;
			unsigned char dirty_properties : 1;
		};
		unsigned char dirty_flags = 0;
	};
};
struct DemoClientFlags {
	union {
		struct {
			unsigned char login : 1;
			unsigned char logout : 1;
			unsigned char update_mob : 1;
			unsigned char update_view : 1;
			unsigned char update_eye : 1;
		};
		unsigned char flags = 0;
	};
};

std::vector<DemoPlayerInfo> demo_player_infos;

DemoPlayerInfo& get_demo_player_info(unsigned short id) {
	if (id >= demo_player_infos.size())demo_player_infos.resize(id + 1);
	return demo_player_infos[id];
}

void init_clients() {
	for (int i = 0; i < Core::client_table->length; i++) {
		if (Core::client_table->elements[i] != nullptr) {
			mark_client_login(i);
		}
	}
	flush_clients();
}

void flush_clients() {
	for (int i = 0; i < demo_player_infos.size; i++) {
		if (demo_player_infos[i].dirty_flags) {
			flush_client(i);
		}
	}
}

void flush_client(unsigned short id) {
	if (id == 0xFFFF) return;
	Value client(DataType::CLIENT, id);
	DemoPlayerInfo& info = get_demo_player_info(id);
	if (!info.dirty_flags) return;
	update_demo_time();
	std::vector<unsigned char> buf;
	write_primitive(buf, id);
	ByteVecRef<DemoClientFlags> dcf = write_primitive(buf, DemoClientFlags());
	if (info.dirty_logout) {
		info.dirty_flags = 0;
		info.last_mob = Value::Null();
		info.last_eye = Value::Null();
		dcf->logout = true;
	}
	else {
		if (info.dirty_login) {
			dcf->login = true;
			Value ckey = client.get_by_id(71);
			Value key = client.get_by_id(70);
			write_byond_string(buf, ckey.type == DataType::STRING ? ckey.value : 0xFFFF);
			write_byond_string(buf, key.type == DataType::STRING ? key.value : 0xFFFF);
		}
		Value mob = client.get_by_id(80);
		if (mob != info.last_mob) {
			info.last_mob = mob;
			info.last_eye = mob;
			dcf->update_mob = true;
			unsigned int ref = ((char)mob.type << 24 | mob.value);
			write_primitive(buf, ref);
		}
		unsigned char view_width = 15, view_height = 15;
		Value view = client.get_by_id(96);
		if (view.type == DataType::NUMBER) {
			view_height = view_width = view.valuef * 2 + 1;
		}
		else if(view.type == DataType::STRING) {
			std::string str = Core::GetStringFromId(view.value);
			int x_index = str.find('x');
			view_width = std::stoi(str.substr(0, x_index));
			view_height = std::stoi(str.substr(x_index + 1, str.length - x_index - 1));
		}
		if (view_width != info.last_view_width || view_height != info.last_view_height) {
			dcf->update_view = true;
			buf.push_back(view_width);
			buf.push_back(view_height);
			info.last_view_width = view_width;
			info.last_view_height = view_height;
		}
		Value eye = client.get_by_id(80);
		if (eye != info.last_eye) {
			dcf->update_eye = true;
			info.last_eye = eye;
			unsigned int ref = ((char)eye.type << 24 | eye.value);
			write_primitive(buf, ref);
		}
		info.dirty_flags = 0;
	}
	if (!dcf->flags) return;
	demo_file_handle.put(6); // Chunk ID
	write_vlq(buf.size());
	demo_file_handle.write((char*)&buf[0], buf.size());
}

void mark_client_login(unsigned short id) {
	if (id == 0xFFFF) return;
	DemoPlayerInfo& info = get_demo_player_info(id);
	if (info.dirty_logout) {
		flush_client(id);
	}
	info.dirty_login = true;
	info.dirty_properties = true;
}
void mark_client_logout(unsigned short id) {
	if (id == 0xFFFF) return;
	DemoPlayerInfo& info = get_demo_player_info(id);
	if (info.dirty_login) {
		info.dirty_flags = 0;
		return;
	}
	info.dirty_logout = true;
}

void mark_client_dirty(unsigned short id) {
	if (id == 0xFFFF) return;
	DemoPlayerInfo& info = get_demo_player_info(id);
	info.dirty_properties = true;
}
