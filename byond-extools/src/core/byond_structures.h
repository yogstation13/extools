#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "byond_constants.h"

#ifdef _WIN32
#define REGPARM3
#define REGPARM2
#else
#define REGPARM3 __attribute__((regparm(3)))
#define REGPARM2 __attribute__((regparm(2)))
#endif
#define FLAG_PROFILE 0x10000

#define PROC_FLAG_HIDDEN 1

struct String
{
	char* stringData;
	int unk1;
	int unk2;
	unsigned int refcount;
};

struct trvh //temporary return value holder, used for sidestepping the fact that structs with constructors are passed in memory and not in eax/ecx when returning them
{
	DataType type;
	union
	{
		int value;
		float valuef;
	};
};

namespace Core
{
	struct ManagedString;
}

struct ManagedValue;


struct Value
{
	DataType type;
	union
	{
		int value;
		float valuef;
	};
	Value() { type = DataType::NULL_D; value = 0; }
	Value(DataType type, int value) : type(type), value(value) {};
	Value(trvh trvh)
	{
		type = trvh.type;
		if (type == DataType::NUMBER)
			value = trvh.value;
		else
			valuef = trvh.valuef;
	}
	Value(float valuef) : type(DataType::NUMBER), valuef(valuef) {};
	Value(std::string s);
	Value(const char* s);
	Value(Core::ManagedString& ms);


	inline static trvh Null() {
		return { DataType::NULL_D, 0 };
	}

	inline static trvh True()
	{
		trvh t { DataType::NUMBER };
		t.valuef = 1.0f;
		return t;
	}

	inline static trvh False()
	{
		return { DataType::NUMBER, 0 };
	}

	inline static Value Global()
	{
		return { DataType::WORLD_D, 0x01 };
	}

	inline static Value World()
	{
		return { DataType::WORLD_D, 0x00 };
	}

	/* inline static Value Tralse()
	{
		return { 0x2A, rand() % 1 };
	} */

	operator trvh()
	{
		return trvh{ type, value };
	}

	bool operator==(const Value& rhs)
	{
		return value == rhs.value && type == rhs.type;
	}

	bool operator!=(const Value& rhs)
	{
		return !(*this == rhs);
	}

	Value& operator +=(const Value& rhs);
	Value& operator -=(const Value& rhs);
	Value& operator *=(const Value& rhs);
	Value& operator /=(const Value& rhs);
	/*inline Value& operator +=(float rhs);
	inline Value& operator -=(float rhs);
	inline Value& operator *=(float rhs);
	inline Value& operator /=(float rhs);*/

	operator std::string();
	operator float();
	operator void*();
	ManagedValue get(std::string name);
	ManagedValue get_safe(std::string name);
	ManagedValue get_by_id(int id);
	ManagedValue invoke(std::string name, std::vector<Value> args, Value usr = Value::Null());
	ManagedValue invoke_by_id(int id, std::vector<Value> args, Value usr = Value::Null());
	std::unordered_map<std::string, Value> get_all_vars();
	bool has_var(std::string name);
	void set(std::string name, Value value);
};

struct ManagedValue : Value
{
	//This class is used to prevent objects being garbage collected before you are done with them
	ManagedValue() = delete;
	ManagedValue(Value val);
	ManagedValue(DataType type, int value);
	ManagedValue(trvh trvh);
	ManagedValue(std::string s);
	ManagedValue(const ManagedValue& other);
	ManagedValue(ManagedValue&& other) noexcept;
	ManagedValue& operator =(const ManagedValue& other);
	ManagedValue& operator =(ManagedValue&& other) noexcept;
	~ManagedValue();
};

struct IDArrayEntry
{
	short size;
	int unknown;
	int refcountMaybe;
};

enum class RbtColor : bool
{
	Black = false,
	Red = true,
};

enum class AppearanceRbtColor : bool
{
	Black = true,
	Red = false,
};

struct AssociativeListEntry
{
	Value key;
	Value value;
	AssociativeListEntry* left;
	AssociativeListEntry* right;
	RbtColor color;
};

struct RawList
{
	Value* vector_part;
	AssociativeListEntry* map_part;
	int allocated_size; //maybe
	int length;
	int refcount;
	int unk3; //this one appears to be a pointer to a struct holding the vector_part pointer, a zero, and maybe the initial size? no clue.

	bool is_assoc()
	{
		return map_part != nullptr;
	}

};

struct DatumVarEntry
{
	int fuck;
	unsigned int id;
	Value value;
};

struct RawDatum
{
	int type_id;
	DatumVarEntry *vars;
	short len_vars; // maybe
	short fuck;
	int cunt;
	int refcount;
};

struct Container;

struct ContainerProxy
{
	Container& c;
	Value key = Value::Null();
	ContainerProxy(Container& c, Value key) : c(c), key(key) {}

	operator Value();
	void operator=(Value val);
};

struct Container //All kinds of lists, including magical snowflake lists like contents
{
	Container();
	Container(DataType type, int id);
	Container(Value val);
	~Container();
	DataType type;
	int id;

	Value at(unsigned int index);
	Value at(Value key);

	unsigned int length();

	operator Value()
	{
		return { type, id };
	}

	operator trvh()
	{
		return { type, id };
	}

	ContainerProxy operator[](unsigned int index)
	{
		return ContainerProxy(*this, Value((float)(index + 1)));
	}

	ContainerProxy operator[](Value key)
	{
		return ContainerProxy(*this, key);
	}
};

struct List //Specialization for Container with fast access by index
{
	List();
	List(int _id);
	List(Value v);
	~List();
	RawList* list;

	int id;

	Value at(int index);
	Value at(Value key);
	void append(Value val);

	bool is_assoc()
	{
		return list->is_assoc();
	}

	Value* begin() { return list->vector_part; }
	Value* end() { return list->vector_part + list->length; }

	operator trvh()
	{
		return { DataType::LIST, id };
	}

	operator Container()
	{
		return { DataType::LIST, id };
	}
};

struct Type
{
	unsigned int path;
	unsigned int parentTypeIdx;
	unsigned int last_typepath_part;
};

struct ProcArrayEntry
{
	int procPath;
	int procName;
	int procDesc;
	int procCategory;
	int procFlags;
	int unknown1;
	int bytecode_idx; // ProcSetupEntry index
	int local_var_count_idx; // ProcSetupEntry index
	int params_idx;
};

struct ExecutionContext;

struct ProcConstants
{
	int proc_id;
	int flags;
	Value usr;
	Value src;
	ExecutionContext* context;
	int sequence_number;
	int unknown4; //some callback thing
	union
	{
		int unknown5;
		int extended_profile_id;
	};
	int arg_count;
	Value* args;
	char unknown6[88];
	int time_to_resume;
};

typedef ProcConstants SuspendedProc;

struct ExecutionContext
{
	ProcConstants* constants;
	ExecutionContext* parent_context;
	std::uint32_t dbg_proc_file;
	std::uint32_t dbg_current_line;
	std::uint32_t* bytecode;
	std::uint16_t current_opcode;
	char test_flag;
	char unknown1;
	Value cached_datum;
	Value unknown2;
	char unknown3[8];
	Value dot;
	Value* local_variables;
	Value* stack;
	std::uint16_t local_var_count;
	std::uint16_t stack_size;
	void* unknown4;
	Value* current_iterator;
	std::uint32_t iterator_allocated;
	std::uint32_t iterator_length;
	std::uint32_t iterator_index;
	Value iterator_filtered;
	char unknown5;
	char iterator_unknown;
	char unknown6;
	std::uint32_t infinite_loop_count;
	char unknown7[2];
	bool paused;
	char unknown8[51];
};

struct BytecodeEntry
{
	std::uint16_t bytecode_length;
	std::uint32_t* bytecode;
	std::uint32_t unknown;
};

struct LocalVarsEntry
{
	std::uint16_t count;
	std::uint32_t* var_name_indices;
	std::uint32_t unknown;
};

struct ParamsData
{
	uint32_t unk_0;
	uint32_t unk_1;
	uint32_t name_index;
	uint32_t unk_2;
};

struct ParamsEntry
{
	std::uint16_t params_count_mul_4;
	ParamsData* params;
	std::uint32_t unknown;

	std::uint32_t count()
	{
		return params_count_mul_4 / 4;
	}
};

struct MiscEntry
{
	union
	{
		ParamsEntry parameters;
		LocalVarsEntry local_vars;
		BytecodeEntry bytecode;
	};
};

struct ProfileEntry
{
	std::uint32_t seconds;
	std::uint32_t microseconds;

	unsigned long long as_microseconds()
	{
		return 1000000 * (unsigned long long)seconds + microseconds;
	}
	double as_seconds()
	{
		return (double)seconds + ((double)microseconds / 1000000);
	}
};

struct ProfileInfo
{
	std::uint32_t call_count;
	ProfileEntry real;
	ProfileEntry total;
	ProfileEntry self;
	ProfileEntry overtime;
	std::uint32_t proc_id;
};

struct NetMsg //named after the struct ThreadedNetMsg - unsure if it's actually that struct
{
	std::uint32_t type;
	std::uint32_t payload_length;
	std::uint32_t unk1;
	std::uint32_t unk2;
	char* payload;
	std::uint32_t unk3;
	std::uint32_t raw_header;
};

struct BSocket //or client?
{
	std::uint32_t unk1;
	std::uint32_t addr_string_id;
	//more unknown fields here
	//EAX + 0x444 is the refcount, holy crap!
	//EAX + 0x54 - key/username
	std::string addr();
};

struct Hellspawn
{
	std::uint32_t outta_my_way;
	std::uint32_t shove_off;
	std::uint32_t handle;
};

struct TableHolderThingy
{
	unsigned int length;
	unsigned int* elements; //?????
};

struct TableHolder2
{
	void** elements;
	unsigned int length;
};

template<class T>
struct RefTable
{
	RefTable(T*** e, unsigned int* l) : elements(*e), length(*l) {}
	RefTable(TableHolder2* th) : elements(*(T***)&th->elements), length(th->length) {}
	RefTable() : elements(dummy_elements), length(dummy_length) {}
	T**& elements;
	unsigned int& length;
private:
	T** dummy_elements = nullptr;
	unsigned int dummy_length = 0;
};

struct VarListEntry;

struct UnknownSimpleLinkedListEntry
{
	unsigned int value;
	UnknownSimpleLinkedListEntry* next;
};

struct UnknownComplexLinkedListEntry
{
	char unknown[0x34];
	UnknownComplexLinkedListEntry* next;
	char unknown2[8];
};

struct TableHolder3
{
	void* elements;
	std::uint32_t size;
	std::uint32_t capacity;
	TableHolder3* next; //probably?
	char unknown[8];
};

struct Obj
{
	trvh loc;
	char unknown[8];
	short bound_x;
	short bound_y;
	short bound_width;
	short bound_height;
	float step_x;
	float step_y;
	float step_size;
	UnknownComplexLinkedListEntry* some_other_linked_list;
	VarListEntry* modified_vars;
	std::uint16_t modified_vars_count;
	std::uint16_t modified_vars_capacity;
	char unknown2[2];
	short pixel_x;
	short pixel_y;
	short pixel_w;
	short pixel_z;
	char unknown3[2];
	UnknownSimpleLinkedListEntry* some_linked_list;
	TableHolder3* vis_contents;
	TableHolder3* vis_locs;
	char unknown4[8];
	int appearance;
	int appearance2;
	int appearance3;
	char unknown5[64];
};

struct Datum
{
	std::uint32_t type;
	VarListEntry* modified_vars;
	std::uint16_t modifier_vars_count;
	std::uint16_t modified_vars_capacity;
	std::uint32_t flags;
	std::uint32_t refcount;
};

struct Mob
{
	trvh loc;
	char unknown[0x8];
	short bound_x;
	short bound_y;
	short bound_width;
	short bound_height;
	float step_x;
	float step_y;
	float step_size;
	UnknownComplexLinkedListEntry* unknown_list1;
	VarListEntry* modified_vars;
	std::uint16_t modified_vars_count;
	std::uint16_t modified_vars_capacity;
	char unknown2[0x2];
	short pixel_x;
	short pixel_y;
	short pixel_w;
	short pixel_z;
	char unknown3[0x2];
	UnknownSimpleLinkedListEntry* unknown_list2;
	TableHolder3* vis_contents;
	TableHolder3* vis_locs;
	char unknown4[0x8];
	int appearance;
	int appearance2;
	int appearance3;
	char unknown5[0x4C];
	UnknownSimpleLinkedListEntry* unknown_list3;
	char unknown6[0x10];
};

struct Turf { // According to lummox, this struct also includes info about atoms overhanging and animations too
	int id;
	Turf* next;
	int obj_contents;
	int mob_contents;
	int unk_10;
	TableHolder3* vis_contents; // vis_contents
	TableHolder3* vis_locs;
	int unk_1c;
	int unk_20;
	int unk_24;
	int unk_28;
	int unk_2c;
};
struct TurfVars {
	int id;
	TurfVars* next;
	VarListEntry* modified_vars;
	std::uint16_t modified_vars_count;
	std::uint16_t modified_vars_capacity;
};

struct TurfSharedInfo {
	int typepath_id;
	int appearance;
	int area;
	int unk_0c;
	short unk_10;
	short unk_12;
	int unk_14;
	short unk_18;
	short unk_1a;
	int unk_1c;
};

struct TurfTableHolder {
	int* shared_info_id_table;
	unsigned char* existence_table;
	int turf_count;
	int maxx;
	int maxy;
	int maxz;
};
struct TurfHashtableHolder {
	Turf** elements;
	int size;
	int mask;
};

struct VarListEntry
{
	std::uint32_t unknown;
	std::uint32_t name_id;
	trvh value;
};

struct Appearance
{
	Appearance* left_node;
	Appearance* right_node;
	Appearance* parent_node;
	Appearance* prev_node;
	Appearance* next_node;
	int id;
	AppearanceRbtColor node_color;
	int name_str;
	int desc_str;
	int suffix_str;
	int screen_loc_str;
	int text_str;
	int icon_res;
	int icon_state_str;
	int overlays_list;
	int underlays_list;
	int verbs_list;
	int unk_44;

	unsigned int opacity : 1;
	unsigned int density : 1;
	unsigned int unk_48_4 : 4;
	unsigned int gender : 2;

	unsigned int mouse_drop_zone : 1;
	unsigned int dir_override : 1; // internal flag for whether dir is inherited or not
	unsigned int unk_49_4 : 2;
	unsigned int mouse_opacity : 2;
	unsigned int animate_movement : 2; // add one and bitwise-and 3 to get actual value
	
	unsigned int unk_4a : 2;
	unsigned int override : 1;
	unsigned int unk_4a_8 : 3;
	
	unsigned int appearance_flags : 10;
	
	unsigned char dir;
	unsigned char invisibility;
	unsigned char infra_luminosity;
	unsigned char luminosity;
	
	short pixel_x;
	short pixel_y;
	
	short pixel_w;
	short pixel_z;
	
	float glide_size;
	float layer;
	int maptext_str;
	
	short maptext_x;
	short maptext_y;
	
	short maptext_width;
	short maptext_height;
	
	Value mouse_over_pointer;
	Value mouse_drag_pointer;
	Value mouse_drop_pointer;
	int unk_84; // an appearance
	int unk_88; // an appearance
	int unk_8c; // an appearance
	
	float transform[6];
	
	union {
		struct {
			unsigned char color_r;
			unsigned char color_g;
			unsigned char color_b;
			unsigned char alpha;
		};
		unsigned int color_alpha;
	};
	
	
	short blend_mode;
	short plane;
	
	float *color_matrix;
	int unk_b4; // probably filters imo
	int render_source_str;
	int render_target_str;
	
	unsigned short vis_flags;
	short unk_c2;
	
	int unk_c4;
	int unk_c8;
	int unk_cc;
	int unk_d0;
	int unk_d4;
	int unk_d8;
	int unk_dc;
	int refcount;
};

struct AppearanceTable
{
	char unk[0x40];
	Appearance** elements;
	int length;
};

struct AppearanceList // used for overlays, underlays, etc
{
	short len;
	int* ids;
};

struct SuspendedProcList
{
	ProcConstants** buffer;
	std::uint32_t front;
	std::uint32_t back;
	std::uint32_t max_elements;
};
