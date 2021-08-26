#include "core.h"
#include "find_functions.h"
#include "socket/socket.h"
#include "../datum_socket/datum_socket.h"
#include <fstream>
#include <unordered_set>
#include <chrono>

ExecutionContext** Core::current_execution_context_ptr;
MiscEntry** Core::misc_entry_table;

RawDatum*** Core::datum_pointer_table;
unsigned int* Core::datum_pointer_table_length;

int ByondVersion;
int ByondBuild;
unsigned int Core::extended_profiling_insanely_hacky_check_if_its_a_new_call_or_resume;

//std::vector<bool> Core::codecov_executed_procs;

std::map<unsigned int, opcode_handler> Core::opcode_handlers;
std::map<std::string, unsigned int> Core::name_to_opcode;
unsigned int next_opcode_id = 0x1337;
bool Core::initialized = false;
unsigned int* Core::name_table_id_ptr = nullptr;
unsigned int* Core::name_table = nullptr;
std::unordered_map<std::string, Value*> Core::global_direct_cache;

Core::ManagedString::ManagedString(unsigned int id) : string_id(id)
{
	string_entry = GetStringTableEntry(string_id);
	IncRefCount(DataType::STRING, string_id);
}

Core::ManagedString::ManagedString(std::string str)
{
	string_id = GetStringId(str);
	string_entry = GetStringTableEntry(string_id); //leaving it like this for now, not feeling like reversing the new structure when we have refcount functions
	IncRefCount(DataType::STRING, string_id);
}

Core::ManagedString::ManagedString(const ManagedString& other)
{
	string_id = other.string_id;
	string_entry = other.string_entry;
	IncRefCount(DataType::STRING, string_id);
}

Core::ManagedString::~ManagedString()
{
	DecRefCount(DataType::STRING, string_id);
}

Core::ManagedString Core::GetManagedString(std::string str)
{
	return ManagedString(str);
}

Core::ResumableProc::ResumableProc(const ResumableProc& other)
{
	proc = other.proc;
}

Core::ResumableProc Core::ResumableProc::FromCurrent()
{
	auto ctx = Core::get_context();
	ctx->current_opcode++;
	auto ret = ResumableProc(Suspend(ctx, 0));
	ctx->current_opcode--;
	return ret;
}

void Core::ResumableProc::resume()
{
	if (!proc)
	{
		return;
	}
	proc->time_to_resume = 1;
	StartTiming(proc);
	proc = nullptr;
}

Core::ResumableProc Core::SuspendCurrentProc()
{
	return ResumableProc::FromCurrent();
}

bool Core::initialize()
{
	if (initialized)
	{
		return true;
	}
	initialized = verify_compat() && find_functions() && populate_proc_list() && hook_custom_opcodes();
	//Core::codecov_executed_procs.resize(Core::get_all_procs().size());
	return initialized;
}

void Core::Alert(const std::string& what) {
#ifdef _WIN32
	MessageBoxA(NULL, what.c_str(), "Ouch!", MB_OK);
#else
	printf("Ouch!: %s\n", what.c_str());
#endif
}

void Core::Alert(int what)
{
	Alert(std::to_string(what));
}

unsigned int Core::GetStringId(std::string str, bool increment_refcount) {
	if (ByondVersion == 514) {
		return GetStringTableIndexUTF8(str.c_str(), 0xFFFFFFFF, 0, 1);
	}
	return 0;
}

std::string Core::GetStringFromId(unsigned int id)
{
	return GetStringTableEntry(id)->stringData;
}

RawDatum* Core::GetDatumPointerById(unsigned int id)
{
	if (id >= *datum_pointer_table_length) return nullptr;
	return (*datum_pointer_table)[id];
}

std::uint32_t Core::register_opcode(std::string name, opcode_handler handler)
{
	std::uint32_t next_opcode = next_opcode_id++;
	opcode_handlers[next_opcode] = handler;
	name_to_opcode[name] = next_opcode;
	return next_opcode;
}

ExecutionContext* Core::get_context()
{
	return *current_execution_context_ptr;
}

Value Core::get_stack_value(unsigned int which)
{
	return (*Core::current_execution_context_ptr)->stack[(*Core::current_execution_context_ptr)->stack_size - which - 1];
}

void Core::stack_pop(unsigned int how_many)
{
	(*Core::current_execution_context_ptr)->stack_size -= how_many;
}

void Core::stack_push(Value val)
{
	(*Core::current_execution_context_ptr)->stack_size++;
	(*Core::current_execution_context_ptr)->stack[(*Core::current_execution_context_ptr)->stack_size-1] = val;
}

std::string Core::type_to_text(unsigned int type)
{
	return GetStringFromId(GetTypeById(type)->path);
}

Value Core::get_turf(int x, int y, int z)
{
	return GetTurf(x-1, y-1, z-1);
}

std::string Core::stringify(Value val)
{
	return GetStringFromId(ToString(val.type, val.value));
}

/*extern "C" __declspec(dllexport) const char* dump_codecov(int a, const char** b)
{
	std::ofstream o("codecov.txt");
	unsigned int called = 0;
	unsigned int actual_count = 0;
	for (int i = 0; i < Core::codecov_executed_procs.size(); i++)
	{
		Core::Proc& p = Core::get_proc(i);
		if (!p.name.empty() && p.name.back() != ')')
		{
			o << p.name << ": " << Core::codecov_executed_procs[i] << "\n";
			if (Core::codecov_executed_procs[i]) called++;
			actual_count++;
		}
	}
	o << "Coverage: " << (called / (float)actual_count) * 100.0f << "% (" << called << "/" << actual_count << ")\n";
	return "";
}*/

std::uint32_t Core::get_socket_from_client(unsigned int id)
{
	int str = (int)GetSocketHandleStruct(id);
	return ((Hellspawn*)(str - 0x74))->handle;
}

void Core::disconnect_client(unsigned int id)
{
#ifdef _WIN32
	closesocket(get_socket_from_client(id));
#else
	close(get_socket_from_client(id));
#endif
	DisconnectClient1(id, 1, 1);
	DisconnectClient2(id);
}

void Core::alert_dd(std::string msg)
{
	msg += "\n";
	PrintToDD(msg.c_str());
}

void Core::cleanup()
{
	Core::remove_all_hooks();
	Core::opcode_handlers.clear();
	Core::destroy_proc_list();
	proc_hooks.clear();
	global_direct_cache.clear();
	clean_sockets();
	Core::initialized = false; // add proper modularization already
}
