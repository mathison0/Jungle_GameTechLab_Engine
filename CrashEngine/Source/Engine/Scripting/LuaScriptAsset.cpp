#include "LuaScriptAsset.h"

#include <fstream>
#include <sstream>

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"

FLuaScriptAsset::FLuaScriptAsset(FString InRelativePath, FString InFullPath)
	: RelativePath(std::move(InRelativePath))
	, FullPath(std::move(InFullPath))
{
}

bool FLuaScriptAsset::Load(sol::state& Lua)
{
	const bool bResult = LoadInternal(Lua);
	if (!bResult)
	{
		UE_LOG([Lua], Error, "Failed to load Lua script '%s': %s",
			FullPath.c_str(), LastError.c_str());
	}

	return bResult;
}

bool FLuaScriptAsset::ReloadIfChange(sol::state& Lua)
{
	if (!HasFileChanged())
	{
		return State == ELuaScriptAssetState::Loaded;
	}

	const bool bResult = LoadInternal(Lua);
	if (!bResult)
	{
		UE_LOG([Lua], Error, "Failed to reload Lua script '%s' : %s",
			FullPath.c_str(), LastError.c_str());
	}

	return bResult;
}

bool FLuaScriptAsset::HasFileChanged() const
{
	std::error_code Ec;
	const auto CurrentWriteTime = std::filesystem::last_write_time(FPaths::ToPath(FullPath), Ec);
	const bool bCurrentFileExists = !Ec;

	if (!bHasObservedFileState)
	{
		return true;
	}

	if (bCurrentFileExists != bLastFileExists)
	{
		return true;
	}

	if (!bCurrentFileExists)
	{
		return false;
	}

	return CurrentWriteTime != LastWriteTime;
}

sol::table FLuaScriptAsset::CreateInstance(sol::state& Lua) const
{
	sol::table Instance = Lua.create_table();

	if (!Prototype.valid())
	{
		return Instance;
	}

	for (const auto& Pair : Prototype)
	{
		Instance[Pair.first] = Pair.second;
	}

	Instance["__assetPath"] = RelativePath;
	Instance["__assetFullPath"] = FullPath;
	Instance["__assetVersion"] = Version;

	return Instance;
}

bool FLuaScriptAsset::LoadInternal(sol::state& Lua)
{
	LastError.clear();

	const std::filesystem::path ScriptFilePath = FPaths::ToPath(FullPath);
	std::ifstream File(ScriptFilePath, std::ios::binary);
	if (!File.is_open())
	{
		LastError = "Failed to open Lua script";
		std::error_code ExistsEc;
		const bool bExists = std::filesystem::exists(ScriptFilePath, ExistsEc);
		State = (!ExistsEc && !bExists) ? ELuaScriptAssetState::Missing : ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	FString NewSourceCode = Buffer.str();

	const FString ChunkName = "@" + RelativePath;

	sol::environment Env(Lua, sol::create, Lua.globals());
	sol::load_result Loaded = Lua.load(NewSourceCode, ChunkName);

	if (!Loaded.valid())
	{
		sol::error Err = Loaded;
		LastError = Err.what();
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::protected_function Function = Loaded;
	
	if (!sol::set_environment(Env, Function))
	{
		LastError = "Failed to set Lua environment";
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::protected_function_result Result = Function();

	if (!Result.valid())
	{
		sol::error Err = Result;
		LastError = Err.what();
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::object Returned = Result.get<sol::object>();
	if (!Returned.is<sol::table>())
	{
		LastError = "Lua script must return a table";
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::table NewPrototype = Returned.as<sol::table>();

	SourceCode = std::move(NewSourceCode);
	Prototype = NewPrototype;
	UpdateObservedFileState();
	++Version;
	State = ELuaScriptAssetState::Loaded;

	UE_LOG([Lua], Info, "Loaded Lua script: %s", RelativePath.c_str());
	return true;
}

bool FLuaScriptAsset::UpdateObservedFileState()
{
	std::error_code Ec;
	LastWriteTime = std::filesystem::last_write_time(FPaths::ToPath(FullPath), Ec);
	bHasObservedFileState = true;
	bLastFileExists = !Ec;
	if (!bLastFileExists)
	{
		LastWriteTime = {};
	}
	return bLastFileExists;
}
