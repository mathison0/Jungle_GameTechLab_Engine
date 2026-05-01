#pragma once
#include <filesystem>
#include <memory>
#include <Sol/forward.hpp>

#include "Core/CoreTypes.h"

class FLuaScriptAsset;

class FScriptSystem
{
public:
	FScriptSystem();
	~FScriptSystem();

	bool Initialize();
	void Shutdown();

	sol::state& GetLua() const { return *Lua; }

	void ScanScripts();
	void LoadAllScripts();
	void ReloadChangedScripts() const;
	
	FLuaScriptAsset* GetScriptAsset(const FString& RelativePath);
	const TArray<FString>& GetAvailableScriptPaths() const { return AvailableScriptPaths; }

	sol::table CreateScriptInstance(const FString& RelativePath);
private:
	FString MakeScriptRelativePath(const std::filesystem::path& FullPath) const;

	void RegisterEngineAPI() const;
	
private:
	std::unique_ptr<sol::state> Lua;

	TMap<FString, std::unique_ptr<FLuaScriptAsset>> ScriptAssets;
	TArray<FString> AvailableScriptPaths;
};
