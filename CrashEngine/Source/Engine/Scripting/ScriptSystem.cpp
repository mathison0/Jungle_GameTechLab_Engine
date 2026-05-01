#include "ScriptSystem.h"

#include <Sol/sol.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

#include "Core/Logging/LogMacros.h"

FScriptSystem::FScriptSystem()
{
}

FScriptSystem::~FScriptSystem()
{
	Shutdown();
}

bool FScriptSystem::Initialize()
{
	Lua = std::make_unique<sol::state>();

	// TODO : 나중에 필요하면 추가하기
	// sol::lib::package
	// sol::lib::debug
	// sol::lib::coroutine
	Lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

	RegisterEngineAPI();

	return true;
}

void FScriptSystem::Shutdown()
{
	Lua.reset();
}

bool FScriptSystem::ExecuteString(const FString& Code) const
{
	sol::load_result Loaded = Lua->load(Code);

	if (!Loaded.valid())
	{
		sol::error Err = Loaded;
		UE_LOG([Lua], Error, "Lua Load Error : %s", Err.what());
		return false;
	}

	sol::protected_function Function = Loaded;
	sol::protected_function_result Result = Function();

	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG([Lua], Error, "Lua Runtime Error : %s", Err.what());
		return false;
	}

	return true;
}

bool FScriptSystem::ExecuteFile(const FString& Path)
{
	// TODO : FPaths와 FResourceManager와 연동해야할 거 같음.
	std::ifstream File(Path);

	if (!File.is_open())
	{
		UE_LOG([Lua], Error, "Failed to open : %s", Path.c_str());
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();

	FString Code = Buffer.str();
	FString ChunkName = "@" + Path;

	sol::load_result Loaded = Lua->load(Code, ChunkName);

	if (!Loaded.valid())
	{
		sol::error Err = Loaded;
		UE_LOG([Lua], Error, "Lua Load Error : %s", Err.what());
		return false;
	}

	sol::protected_function Function = Loaded;
	sol::protected_function_result Result = Function();

	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG([Lua], Error, "Lua Runtime Error : %s", Err.what());
		return false;
	}

	return true;
}

sol::state& FScriptSystem::GetLua()
{
	return *Lua;
}

void FScriptSystem::RegisterEngineAPI()
{
	Lua->set_function("Log", [](const FString& Message)
		{
			UE_LOG([Lua], Info, "%s", Message.c_str());
		});
}