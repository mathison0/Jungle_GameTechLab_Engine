#include "ScriptSystem.h"

#include <Sol/sol.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ranges>

#include "LuaScriptAsset.h"
#include "Core/Logging/LogMacros.h"
#include "Input/GameInput.h"
#include "Platform/Paths.h"
#include "LuaEngineBinding.h"
#include "GameFramework/World.h"
#include "Runtime/Engine.h"
#include "Sound/SoundManager.h"

namespace
{
constexpr float EditorScriptRefreshInterval = 0.5f;

ESoundBus ParseSoundBus(const FString& BusName)
{
	FString Lower = BusName;
	std::transform(Lower.begin(), Lower.end(), Lower.begin(),
		[](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});

	if (Lower == "master")
	{
		return ESoundBus::Master;
	}
	if (Lower == "bgm")
	{
		return ESoundBus::BGM;
	}
	if (Lower == "ui")
	{
		return ESoundBus::UI;
	}
	if (Lower == "player")
	{
		return ESoundBus::Player;
	}
	if (Lower == "ambience" || Lower == "ambient")
	{
		return ESoundBus::Ambience;
	}

	return ESoundBus::SFX;
}

void ReadSoundPlayArgs(const sol::variadic_args& Args, FString& OutBusName, float& OutVolume)
{
	bool bBusRead = false;
	bool bVolumeRead = false;

	for (const sol::object& Arg : Args)
	{
		if (!bBusRead && Arg.is<FString>())
		{
			OutBusName = Arg.as<FString>();
			bBusRead = true;
			continue;
		}

		if (!bVolumeRead && Arg.is<float>())
		{
			OutVolume = Arg.as<float>();
			bVolumeRead = true;
			continue;
		}

		if (!bVolumeRead && Arg.is<double>())
		{
			OutVolume = static_cast<float>(Arg.as<double>());
			bVolumeRead = true;
			continue;
		}

		if (!bVolumeRead && Arg.is<int>())
		{
			OutVolume = static_cast<float>(Arg.as<int>());
			bVolumeRead = true;
		}
	}
}
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
    Lua->open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::math, sol::lib::table, sol::lib::string, sol::lib::package);
    BindPackagePath();

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

	Lua->new_usertype<FSoundHandle>(
		"SoundHandle",
		sol::constructors<FSoundHandle()>(),
		"Id", &FSoundHandle::Id,
		"Generation", &FSoundHandle::Generation,
		"IsValid", &FSoundHandle::IsValid);

	Lua->set_function("PlaySound", [](const FString& Key, const sol::variadic_args& Args) -> FSoundHandle
		{
			FString BusName = "SFX";
			float Volume = 1.0f;
			ReadSoundPlayArgs(Args, BusName, Volume);
			return FSoundManager::Get().Play(FName(Key), ParseSoundBus(BusName), Volume);
		});

	Lua->set_function("PlayLoopSound", [](const FString& Key, const sol::variadic_args& Args) -> FSoundHandle
		{
			FString BusName = "SFX";
			float Volume = 1.0f;
			ReadSoundPlayArgs(Args, BusName, Volume);
			return FSoundManager::Get().PlayLoop(FName(Key), ParseSoundBus(BusName), Volume);
		});

	Lua->set_function("StopSound", [](const FSoundHandle& Handle)
		{
			FSoundManager::Get().Stop(Handle);
		});

	Lua->set_function("StopSoundBus", [](const FString& BusName)
		{
			FSoundManager::Get().StopBus(ParseSoundBus(BusName));
		});

	Lua->set_function("SetSoundBusVolume", [](const FString& BusName, float Volume)
		{
			FSoundManager::Get().SetBusVolume(ParseSoundBus(BusName), Volume);
		});

	Lua->set_function("SetMasterSoundVolume", [](float Volume)
		{
			FSoundManager::Get().SetMasterVolume(Volume);
		});

	Lua->set_function("SetSoundVolume", [](const FSoundHandle& Handle, float Volume)
		{
			FSoundManager::Get().SetSoundVolume(Handle, Volume);
		});

	Lua->set_function("IsSoundPlaying", [](const FSoundHandle& Handle) -> bool
		{
			return FSoundManager::Get().IsPlaying(Handle);
		});
    
    // 입력 처리
    Lua->new_usertype<FGameInput>("Input",
        "GetKey", &FGameInput::GetKey,
        "GetKeyDown", &FGameInput::GetKeyDown,
        "GetKeyUp", &FGameInput::GetKeyUp,
        "GetAxis", &FGameInput::GetAxis,
        "GetMouseButton", &FGameInput::GetMouseButton,
        "GetMouseButtonDown", &FGameInput::GetMouseButtonDown,
        "GetMouseButtonUp", &FGameInput::GetMouseButtonUp,
        "GetMousePosition", []() {
            int x, y;
            FGameInput::GetMousePosition(x, y);
            return std::make_tuple(x, y);
        }
    );    

	Lua->set_function("GetActorPoolManager", []() {
		UWorld* World = GEngine->GetWorld();
		return FLuaActorPoolManagerHandle(World ? World->GetPoolManager() : nullptr);
	});

	RegisterLuaEngineBindings(*Lua);
}

void FScriptSystem::BindPackagePath() const
{
    sol::table Package = (*Lua)["package"];

    std::string OldPath = Package["path"].get_or<std::string>("");

    std::string ExtraPath =
        ";Asset/Scripts/?.lua"
        ";Asset/Scripts/?/init.lua";

    Package["path"] = OldPath + ExtraPath;
}
