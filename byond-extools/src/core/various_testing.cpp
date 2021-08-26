#include "core.h"
#include "../dmdism/disassembly.h"
#include "../crash_guard/crash_guard.h"

#include <fstream>

trvh cheap_hypotenuse(Value* args, unsigned int argcount)
{
	return { DataType::NUMBER, (int)sqrt((args[0].valuef - args[2].valuef) * (args[0].valuef - args[2].valuef) + (args[1].valuef - args[3].valuef) * (args[1].valuef - args[3].valuef)) };
}

trvh measure_get_variable(Value* args, unsigned int argcount)
{
	int name_string_id = Core::GetStringId("name");
	DataType type = args[0].type;
	int value = args[0].value;
	//long long duration = 0;
	//for (int j = 0; j < 1000; j++)
	//{
		//auto t1 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10; i++)
	{
		GetVariable(args[0].type, args[0].value, name_string_id);
	}
	//auto t2 = std::chrono::high_resolution_clock::now();
	//auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	//duration += duration2;
//}

//Core::Alert(std::to_string(duration/1000).c_str());
	return Value::Null();
}

trvh show_profiles(Value* args, unsigned int argcount)
{
	unsigned long long dm = Core::get_proc("/proc/measure_dm").profile()->total.microseconds;
	unsigned long long native = Core::get_proc("/proc/measure_native").profile()->total.microseconds;
	std::string woo = "DM proc took " + std::to_string(dm) + " microseconds while native took " + std::to_string(native) + " microseconds.";
	Core::Alert(woo);
	return Value::Null();
}

void cheap_hypotenuse_opcode(ExecutionContext* ctx) //for testing purposes, remove later
{
	//Core::Alert("Calculating hypotenuse");
	const float Ax = Core::get_stack_value(4).valuef;
	const float Ay = Core::get_stack_value(3).valuef;
	const float Bx = Core::get_stack_value(2).valuef;
	const float By = Core::get_stack_value(1).valuef;
	Core::stack_pop(4);
	Core::stack_push({ DataType::NUMBER, (int)std::sqrt((Ax - Bx) * (Ax - Bx) + (Ay - By) * (Ay - By)) });
}



unsigned char _number_op_localm_localn_store_localx[] =
{
	0x55,							// PUSH EBP
	0x8B, 0xEC,						// MOV EBP, ESP
	0x8B, 0x45, 0x08,				// MOV EAX, DWORD PTR SS : [EBP + 0x8]
	0x8B, 0x40, 0x38,				// MOV EAX, DWORD PTR DS : [EAX + 0x38]
	0xF3, 0x0F, 0x10, 0x40, 0x0C,	// MOVSS XMM0, DWORD PTR DS : [EAX + 0xC]
	0xF3, 0x0F, 0x58, 0x40, 0x04,	// ADDSS XMM0, DWORD PTR DS : [EAX + 0x4]
	0xF3, 0x0F, 0x11, 0x40, 0x14,	// MOVSS DWORD PTR DS : [EAX + 0x14], XMM0
	0x5D,							// POP EBP
	0xC3,							// RET
};

template<typename T>
class ext_vector : std::vector<T>
{
public:
	template<typename... Ts>
	void extend(Ts... rest)
	{
		(push_back(rest), ...);
	}
};

extern "C" EXPORT void add_subvars_of_locals(ExecutionContext* ctx)
{
	Value a = ctx->local_variables[0];
	Value b = ctx->local_variables[1];
	ctx->local_variables[2].valuef = GetVariable(a.type, a.value, 0x33).valuef + GetVariable((int)b.type, b.value, 0x33).valuef;
}

trvh toggle_verb_hidden(unsigned int argcount, Value* args, Value src)
{
	Core::get_proc("/client/verb/hidden").proc_table_entry->procFlags = 4;
	return Value::Null();
}

trvh test_invoke(unsigned int argcount, Value* args, Value src)
{
	return Value(DataType::STRING, args[0]);
}

void init_testing()
{
	auto d = Core::get_proc("/client/verb/access_thingy").disassemble();
	std::ofstream o("out.txt");
	for (Instruction& i : d)
	{
		o << i.offset() << "\t\t\t" << i.bytes_str() << "\t\t\t" << i.opcode().mnemonic() << " " << i.comment() << "\n";
	}
}

void run_tests()
{

}

extern "C" EXPORT const char* run_tests(int n_args, const char** args)
{
	if (!Core::initialize())
	{
		return Core::FAIL;
	}
	init_testing();
	run_tests();
	return Core::SUCCESS;
}
