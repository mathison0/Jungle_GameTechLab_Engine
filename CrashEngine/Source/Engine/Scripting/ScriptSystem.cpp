#include "ScriptSystem.h"

#include <Sol/sol.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ranges>

#include "LuaScriptAsset.h"
#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"

namespace
{
constexpr float EditorScriptRefreshInterval = 0.5f;
}

FScriptSystem::FScriptSystem()
= default;

FScriptSystem::~FScriptSystem()
{
	Shutdown();
}

bool FScriptSystem::Initialize()
{
	Lua = std::make_unique<sol::state>();
	EditorRefreshAccumulator = 0.0f;

	// TODO : 나중에 필요하면 추가하기
	// sol::lib::package
	// sol::lib::debug
	// sol::lib::coroutine
    Lua->open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::math, sol::lib::table, sol::lib::string);

	RegisterEngineAPI();

	ScanScripts();
	LoadAllScripts();

	return true;
}

void FScriptSystem::Shutdown()
{
	ScriptAssets.clear();
	AvailableScriptPaths.clear();
	EditorRefreshAccumulator = 0.0f;
	Lua = nullptr;
}

void FScriptSystem::ScanScripts()
{
	AvailableScriptPaths.clear();

	const std::filesystem::path Root = FPaths::ToPath(FPaths::ScriptsDir());

	std::error_code Ec;
	if (!std::filesystem::exists(Root, Ec))
	{
		ScriptAssets.clear();
		return;
	}

	TSet<FString> Seen;

	for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Ec))
	{
	    if (Ec)
	    {
			break;
	    }

		if (!Entry.is_regular_file())
		{
			continue;
		}

		if (Entry.path().extension() != L".lua")
		{
			continue;
		}

		FString RelativePath = MakeScriptRelativePath(Entry.path());
		FString FullPath = FPaths::ToUtf8(Entry.path().wstring());
		Seen.insert(RelativePath);
		AvailableScriptPaths.push_back(RelativePath);

		if (!ScriptAssets.contains(RelativePath))
		{
			ScriptAssets.emplace(RelativePath, std::make_unique<FLuaScriptAsset>(RelativePath, FullPath));
		}
	}

	for (auto It = ScriptAssets.begin(); It != ScriptAssets.end();)
	{
	    if (!Seen.contains(It->first))
	    {
			It = ScriptAssets.erase(It);
	    }
		else
		{
			++It;
		}
	}

	std::ranges::sort(AvailableScriptPaths);
}

void FScriptSystem::LoadAllScripts()
{
	if (!Lua)
	{
		return;
	}

	for (const auto& Asset : ScriptAssets | std::views::values)
	{
		Asset->Load(*Lua);
	}
}

void FScriptSystem::ReloadChangedScripts() const
{
	if (!Lua)
	{
		return;
	}

	for (const auto& Val : ScriptAssets | std::views::values)
	{
		Val->ReloadIfChange(*Lua);
	}
}

void FScriptSystem::TickEditor(float DeltaTime)
{
	if (!Lua)
	{
		return;
	}

	if (DeltaTime > 0.0f)
	{
		EditorRefreshAccumulator += DeltaTime;
	}

	if (EditorRefreshAccumulator < EditorScriptRefreshInterval)
	{
		return;
	}

	EditorRefreshAccumulator = 0.0f;
	ScanScripts();
	ReloadChangedScripts();
}

FLuaScriptAsset* FScriptSystem::GetScriptAsset(const FString& RelativePath)
{
	auto It = ScriptAssets.find(RelativePath);
	if (It == ScriptAssets.end())
	{
		return nullptr;
	}

	return It->second.get();
}

sol::table FScriptSystem::CreateScriptInstance(const FString& RelativePath, const TArray<FLuaScriptPropertyOverride>* PropertyOverrides)
{
	if (!Lua)
	{
		return {};
	}

	FLuaScriptAsset* Asset = GetScriptAsset(RelativePath);
	if (!Asset || !Asset->IsUsable())
	{
		return {};
	}

	return Asset->CreateInstance(*Lua, PropertyOverrides);
}

FString FScriptSystem::MakeScriptRelativePath(const std::filesystem::path& FullPath) const
{
	const std::filesystem::path Root = FPaths::ToPath(FPaths::ScriptsDir());

	std::error_code Ec;
	std::filesystem::path Relative = std::filesystem::relative(FullPath, Root, Ec);
	if (Ec)
	{
		Relative = FullPath.filename();
	}

	return FPaths::ToUtf8(Relative.generic_wstring());
}

void FScriptSystem::RegisterEngineAPI() const
{
	Lua->set_function("Log", [](const FString& Message)
		{
			UE_LOG([Lua], Info, "%s", Message.c_str());
		});
}
