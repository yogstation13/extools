#include "joao.h"
#include "../core/core.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <forward_list>
#include <list>
#include <stack>

namespace // To make absolutely sure no other part of the code tries to link to all this shit
{
	namespace joao
	{
#define JOAO_NO_INCLUDE_STD // A kludge necessary to avoid namespace problems associated with wrapping all of João in this funky namespace block
#define JOAO_SAFE //not-as-much-of-a kludge used to enable throttling and disable some dangerous libraries such as the /file Object type
#include "./joao_repo/AST.cpp"
#include "./joao_repo/Directory.cpp"
#include "./joao_repo/Interpreter.cpp"

#include "./joao_repo/nativefuncs/error.cpp"
#include "./joao_repo/nativefuncs/math.cpp"
#include "./joao_repo/nativefuncs/string.cpp"
#include "./joao_repo/nativefuncs/tablelib.cpp"

#include "./joao_repo/Object.cpp"
#include "./joao_repo/Parser.cpp"
#include "./joao_repo/Program.cpp"
#include "./joao_repo/Scanner.cpp"
#include "./joao_repo/Table.cpp"
#undef JOAO_NO_INCLUDE_STD
#undef JOAO_SAFE
	}
}

//for the sake of being explicit, since there is a minor name collision going on here
using ByondValue = Value;
using JoaoValue = joao::Value;

namespace
{
	namespace ID
	{
		int data;
		int content;
		int message;
		int frequency;
		int name;
	}
}


const char* enable_joao()
{
	// get the var IDs for SANIC SPEED
	ID::data = Core::GetStringId("data", true);
	ID::content = Core::GetStringId("content", true);
	ID::message = Core::GetStringId("message", true);
	ID::frequency = Core::GetStringId("frequency", true);
	ID::name = Core::GetStringId("name", true);

	//Set up hooks
	Core::get_proc("/proc/run_script").hook(run_script);
	return "ok";
}

trvh run_script(unsigned int argn, ByondValue* args, ByondValue src)
{
	using namespace joao;
	if (argn < 2) // If we weren't given a script or signal
		return ByondValue::False();

	ByondValue& script = args[0];
	if (script.type != DataType::STRING) // If what we were given can't be a script
	{
		return ByondValue::False();
	}

	ByondValue& packet = args[1];
	if (packet.type != DataType::DATUM)
		return ByondValue::False();
	ByondValue& signal = args[1].get("signal");
	if (signal.type != DataType::DATUM) // If what we were given isn't even a datum (signals are, this comment upholding, /datum/signal/subspace/vocal)
		return ByondValue::False();

	std::istringstream stream = std::istringstream(std::string(script));

	Scanner scanner;
	scanner.scan(stream);
	Parser pears(scanner);

	
	//Tries to follow the structure of the script_signal var from NTSL
	//The default values are not based on the actual incoming signal, since they are used whenever *any* /signal Object is instantiated within Joao.
	ObjectType signal_type("signal");
	JoaoValue empty_string = JoaoValue(std::string(""));
	signal_type.set_typeproperty_raw("content", empty_string);
	signal_type.set_typeproperty_raw("freq", JoaoValue(0));
	signal_type.set_typeproperty_raw("source", empty_string);

	pears.IncludeAlienType(&signal_type);
	Program parsed = pears.parse();

	Interpreter interpreter(parsed, false);
	
	//Constructing the João Object to be passed as the argument
	Object* signal_obj = signal_type.makeObject(interpreter, {});

	signal_obj->set_property_raw("content", JoaoValue(Core::GetStringFromId(signal.get_by_id(ID::data).get_by_id(ID::message).value)));
	signal_obj->set_property_raw("freq", JoaoValue(JoaoValue::JoaoInt(signal.get_by_id(ID::frequency).valuef))); // I think?
	signal_obj->set_property_raw("source", JoaoValue(Core::GetStringFromId(signal.get_by_id(ID::data).get_by_id(ID::name).value)));

	JoaoValue jargs = interpreter.makeBaseTable({ JoaoValue(signal_obj) }, {}, nullptr);

	//Execute!
	JoaoValue ret;
	try
	{
		ret = interpreter.execute(parsed, jargs);
	}
	catch (const JoaoValue& err)
	{
		ByondValue& error = packet.get("err");
		error.set("what", ByondValue(*(err.t_value.as_object_ptr->get_property_raw("what").t_value.as_string_ptr)));
		error.set("code", ByondValue(err.t_value.as_object_ptr->get_property_raw("code").t_value.as_int));
		return ByondValue::False();
	}
	catch (std::exception exp)
	{
		ByondValue& error = packet.get("err");
		error.set("what", ByondValue(std::string(exp.what())));
		error.set("code", -1);
		return ByondValue::False();
	}
	catch(...) // A real fucking scary thing to put here but I dunno how else to resolve this
	{
		ByondValue& error = packet.get("err");
		error.set("what", "An unknown error occurred!");
		error.set("code", -2);
		return ByondValue::False();
	}
	if (ret.t_vType != JoaoValue::vType::Object || ret.t_value.as_object_ptr->object_type != "signal") // If the program returned something that isn't a signal object
	{
		ByondValue& error = packet.get("err");
		error.set("what", "Returned value was not a /signal!");
		error.set("code", -3);
		return ByondValue::False();
	}
	Object*& retptr = ret.t_value.as_object_ptr; // $#(@$*@!!!!

	//preparing for the agony of setting the elements of a /list
	auto datas = signal.get_by_id(ID::data); // "datas" is the plural of "data," which is in turn the plural of "datum."

	//Content typecheck & set
	JoaoValue& ret_content = retptr->get_property_raw("content");
	if (ret_content.t_vType != JoaoValue::vType::String)
	{
		ByondValue& error = packet.get("err");
		error.set("what", "/signal.content was not of type String!");
		error.set("code", -4);
		return ByondValue::False();
	}
	
	ManagedValue msg = ManagedValue(*ret_content.t_value.as_string_ptr);
	SetAssocElement(datas.type, datas.value, DataType::STRING, ID::message, msg.type, msg.value); // Have to use this raw shit since extools lacks a good API for it

	//Frequency typecheck & set
	JoaoValue& ret_freq = retptr->get_property_raw("freq");
	if (ret_freq.t_vType != JoaoValue::vType::Integer)
	{
		ByondValue& error = packet.get("err");
		error.set("what", "/signal.freq was not of type Integer!");
		error.set("code", -4);
		return ByondValue::False();
	}
	//signal.get_by_id(ID::data).set_by_id(ID::frequency, ret_freq.t_value.as_int);

	ManagedValue freq = ManagedValue(ret_freq.t_value.as_int);
	SetAssocElement(datas.type, datas.value, DataType::STRING, ID::frequency, freq.type, freq.value); // Have to use this raw shit since extools lacks a good API for it
	
	//Source typecheck & set
	JoaoValue& ret_source = retptr->get_property_raw("source");
	if (ret_source.t_vType != JoaoValue::vType::String)
	{
		ByondValue& error = packet.get("err");
		error.set("what", "/signal.source was not of type String!");
		error.set("code", -4);
		return ByondValue::False();
	}
	//signal.get_by_id(ID::data).set_by_id(ID::name, *ret_source.t_value.as_string_ptr);

	ManagedValue source = ManagedValue(*ret_source.t_value.as_string_ptr);
	SetAssocElement(datas.type, datas.value, DataType::STRING, ID::name, source.type, source.value); // Have to use this raw shit since extools lacks a good API for it

	return ByondValue::True();
}


