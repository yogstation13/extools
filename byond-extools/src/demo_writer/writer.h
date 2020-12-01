#pragma once
#include <vector>
#include <queue>
#include <algorithm>
#include <fstream>
#include "../core/core.h"

extern std::ofstream demo_file_handle;

struct alignas(1) DemoWriterIdFlags {
	union {
		struct {
			unsigned char string_written : 1;
			unsigned char appearance_written : 1;
			unsigned char obj_written : 1;
			unsigned char mob_written : 1;
			unsigned char turf_written : 1;
			unsigned char resource_written : 1;
		};
		unsigned char byte = 0;
	};
	template<class T> inline bool get_written();
	template<> inline bool get_written<Obj>() { return obj_written; }
	template<> inline bool get_written<Mob>() { return mob_written; }
	template<> inline bool get_written<Turf>() { return turf_written; }
	
	template<class T> inline void set_written(bool f);
	template<> inline void set_written<Obj>(bool f) { obj_written = f; }
	template<> inline void set_written<Mob>(bool f) { mob_written = f; }
	template<> inline void set_written<Turf>(bool f) { turf_written = f; }
};
DemoWriterIdFlags& get_demo_id_flags(int id);

struct alignas(1) DemoAppearanceFlags {
	union {
		struct {
			unsigned char dir : 3;
			unsigned char opacity : 1;
			unsigned char density : 1;
			unsigned char gender : 2;
			unsigned char dir_override : 1;

			unsigned char include_name : 1;
			unsigned char include_desc : 1;
			unsigned char include_screen_loc : 1;
			unsigned char include_icon : 1;
			unsigned char include_overlays_and_underlays : 1;
			unsigned char include_layer : 1;
			unsigned char include_invisibility : 1;
			unsigned char include_glide_size : 1;

			unsigned char include_pixel_xy : 1;
			unsigned char include_pixel_wz : 1;
			unsigned char include_plane : 1;
			unsigned char include_transform : 1;
			unsigned char include_color_alpha : 1;
			unsigned char include_color_matrix : 1;
			unsigned char animate_movement : 2;
		};
		unsigned char bytes[3] = { 0,0,0 };
	};
};

struct alignas(1) ColorMatrixFormat {
	union {
		struct {
			unsigned char is_float : 1;
			unsigned char include_cr_cg_cb : 1;
			unsigned char include_ca : 1;
			unsigned char ca_one : 1;
			unsigned char include_ar_ag_ab : 1;
			unsigned char include_aa : 1;
			unsigned char include_ra_ga_ba : 1;
			unsigned char include_color : 1;
		};
		unsigned char byte = 0;
	};
};

int write_appearance(std::vector<unsigned char> &buf, int appearance_id);
void update_demo_time();
void write_world_size();
void write_byond_string(std::vector<unsigned char>& buf, int string_id);
void write_vlq(std::vector<unsigned char>& buf, int value);
void write_vlq(int value);

template<class T>
class ByteVecRef {
public:
	ByteVecRef(std::vector<unsigned char>* vec, int num) {
		this->vec = vec;
		this->num = num;
	}
	T* operator->() {
		return (T*)&(*vec)[num];
	}
private:
	std::vector<unsigned char> *vec;
	int num;
};

template<class T>
ByteVecRef<T> write_primitive(std::vector<unsigned char>& buf, const T &value) {
	int n = buf.size();
	buf.resize(n + sizeof(T));
	memcpy(&buf[n], &value, sizeof(T));
	return ByteVecRef<T>(&buf, n);
}

struct alignas(1) DemoAtomFlags {
	union {
		struct {
			unsigned char include_appearance : 1;
			unsigned char include_loc : 1;
			unsigned char include_vis_contents : 1;
			unsigned char include_step_xy : 1;
		};
		unsigned char byte = 0;
	};
};

template<class T> struct LastListItem {
	int last_loc = 0;
	int last_appearance = 0xFFFF;
	short last_step_x = 0;
	short last_step_y = 0;
};
template<> struct LastListItem<Turf> {
	int last_appearance = 0xFFFF;
};
/*template<> struct LastListItem<Image> {
	int last_loc = 0;
	int last_appearance = 0xFFFF;
};*/

template<class Atom, unsigned char update_chunk_id, bool includes_loc = true, bool includes_stepxy = true>
class AtomUpdateBuffer {
private:
	int dirty_floor = 0;
	std::priority_queue<int, std::vector<int>, std::greater<int>> dirty_list;
	std::vector<LastListItem<Atom>> last_list;
	void write_update(std::vector<unsigned char>& buf, int id) {
		DemoWriterIdFlags& dif = get_demo_id_flags(id);
		dif.set_written<Atom>(true);
		Atom* atom = get_element(id);
		ByteVecRef<DemoAtomFlags> daf = write_primitive(buf, DemoAtomFlags());
		if (last_list.size() <= id) {
			last_list.resize(id + 1);
		}
		LastListItem<Atom>& lli = last_list[id];
		int appearance = get_appearance(atom, id);
		if (appearance != lli.last_appearance || !(get_demo_id_flags(appearance).appearance_written)) {
			daf->include_appearance = true;
			lli.last_appearance = write_appearance(buf, appearance);
		}
		if constexpr (includes_loc) {
			trvh loc_val = atom ? atom->loc : Value::Null();
			int loc = ((char)loc_val.type << 24 | loc_val.value);
			if (loc != lli.last_loc) {
				daf->include_loc = true;
				write_primitive(buf, loc);
				lli.last_loc = loc;
			}
		}
		if constexpr (includes_stepxy) {
			if (atom) {
				short step_x = (short)(atom->step_x * 256);
				short step_y = (short)(atom->step_y * 256);
				if (step_x != lli.last_step_x || step_y != lli.last_step_y) {
					daf->include_step_xy = true;
					write_primitive(buf, step_x);
					write_primitive(buf, step_y);
					lli.last_step_x = step_x;
					lli.last_step_y = step_y;
				}
			}
		}
	}

	int next_item() {
		if (!dirty_list.empty()) {
			int popped = dirty_list.top();
			dirty_list.pop();
			return popped;
		}
		while (dirty_floor < get_table_length()) {
			int r = dirty_floor++;
			if (!get_demo_id_flags(r).get_written<Atom>() && does_element_exist(r)) {
				return r;
			}
		}
		return -1;
	}

	inline int get_table_length();
	inline Atom *get_element(int id);
	inline int get_appearance(Atom* atom, int id) { return atom ? atom->appearance : 0xFFFF; }
	inline bool does_element_exist(int id) { return get_element(id); }
public:
	void flush() {
		int curr_ref = next_item();
		if (curr_ref < 0) return;
		std::vector<unsigned char> vec;
		write_vlq(vec, curr_ref);
		int len_pos = vec.size();
		int amt_in = 0;;
		while (curr_ref >= 0) {
			write_update(vec, curr_ref);
			amt_in++;
			int ideal_next = curr_ref + 1;
			curr_ref = next_item();
			while (curr_ref < ideal_next && curr_ref >= 0) {
				curr_ref = next_item(); // dang it going down is bad
			}
			if (curr_ref != ideal_next) {
				std::vector<unsigned char> vlq_vec;
				write_vlq(vlq_vec, amt_in);
				amt_in = 0;
				vec.insert(vec.begin() + len_pos, vlq_vec.begin(), vlq_vec.end());
				if (curr_ref > ideal_next) {
					write_vlq(vec, curr_ref - ideal_next);
				}
				len_pos = vec.size();
			}
		}
		vec.push_back(0);
		update_demo_time();
		demo_file_handle.put(update_chunk_id);
		write_vlq(vec.size());
		demo_file_handle.write((char*)&vec[0], vec.size());
	}

	void mark_dirty(int id) {
		if (id >= dirty_floor) return;
		DemoWriterIdFlags& dif = get_demo_id_flags(id);
		if (dif.get_written<Atom>()) {
			dif.set_written<Atom>(false);
			dirty_list.push(id);
		}
	}
};

inline int AtomUpdateBuffer<Turf, 0x2, false, false>::get_table_length() {
	return Core::turf_table->turf_count;
}
inline int AtomUpdateBuffer<Obj, 0x3, true, true>::get_table_length() {
	return Core::obj_table->length;
}
inline int AtomUpdateBuffer<Mob, 0x4, true, true>::get_table_length() {
	return Core::mob_table->length;
}
inline Turf *AtomUpdateBuffer<Turf, 0x2, false, false>::get_element(int id) {
	if (id < Core::turf_table->turf_count || Core::turf_table->existence_table[id] == 0) {
		return nullptr;
	}
	Turf* ref = Core::turf_hashtable->elements[id & Core::turf_hashtable->mask];
	while (ref && ref->id != id) {
		ref = ref->next;
	}
	return ref;
}
inline Obj* AtomUpdateBuffer<Obj, 0x3, true, true>::get_element(int id) {
	return id < Core::obj_table->length ? Core::obj_table->elements[id] : nullptr;
}
inline Mob* AtomUpdateBuffer<Mob, 0x4, true, true>::get_element(int id) {
	return id < Core::mob_table->length ? Core::mob_table->elements[id] : nullptr;
}
inline int AtomUpdateBuffer<Turf, 0x2, false, false>::get_appearance(Turf* turf, int id) {
	if (id >= Core::turf_table->turf_count) return 0xFFFF;
	int shared_id = Core::turf_table->shared_info_id_table[id];
	return (*Core::turf_shared_info_table)[shared_id]->appearance;
}
inline int AtomUpdateBuffer<Obj, 0x3, true, true>::get_appearance(Obj* obj, int id) {
	return obj ? GetObjAppearance(id) : 0xFFFF;
}
inline int AtomUpdateBuffer<Mob, 0x4, true, true>::get_appearance(Mob* mob, int id) {
	return mob ? GetMobAppearance(id) : 0xFFFF;
}
inline bool AtomUpdateBuffer<Turf, 0x2, false, false>::does_element_exist(int id) {
	return true;
}

extern AtomUpdateBuffer<Turf, 0x2, false, false> turf_update_buffer;
extern AtomUpdateBuffer<Obj, 0x3, true, true> obj_update_buffer;
extern AtomUpdateBuffer<Mob, 0x4, true, true> mob_update_buffer;
