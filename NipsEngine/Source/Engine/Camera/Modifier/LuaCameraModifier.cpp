#include "Engine/Camera/Modifier/LuaCameraModifier.h"

#include "Core/Logger.h"
#include "Core/Paths.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Object/ObjectFactory.h"
#include "Scripting/LuaBindings.h"

#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>

DEFINE_CLASS(ULuaCameraModifier, UCameraModifier)
REGISTER_FACTORY(ULuaCameraModifier)

namespace
{
#if WITH_LUA
bool LoadLuaSourceFromFile(const FString& ScriptPath, FString& OutSource, FString& OutError)
{
	const std::filesystem::path WidePath(FPaths::ToAbsolute(FPaths::ToWide(ScriptPath)));
	std::ifstream File(WidePath, std::ios::binary);
	if (!File.is_open())
	{
		OutError = "failed to open script file.";
		return false;
	}

	OutSource.assign(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
	if (File.bad())
	{
		OutError = "failed to read script file.";
		return false;
	}

	return true;
}

template <typename TSettings>
bool CallLuaModifierFunction(sol::state* LuaState, const FString& ScriptPath, const char* FunctionName, float DeltaTime, TSettings& InOutSettings, FString& OutError)
{
	if (LuaState == nullptr)
	{
		return false;
	}

	sol::protected_function Function = (*LuaState)[FunctionName];
	if (!Function.valid())
	{
		return false;
	}

	sol::protected_function_result Result = Function(std::ref(InOutSettings), DeltaTime);
	if (!Result.valid())
	{
		sol::error Error = Result;
		OutError = Error.what();
		UE_LOG("LuaCameraModifier: failed to call %s in '%s': %s", FunctionName, ScriptPath.c_str(), OutError.c_str());
		return false;
	}

	if (Result.return_count() > 0)
	{
		sol::object ReturnValue = Result.get<sol::object>(0);
		if (ReturnValue.is<bool>())
		{
			return ReturnValue.as<bool>();
		}
	}

	return true;
}
#endif
}

ULuaCameraModifier::~ULuaCameraModifier()
{
	UnloadScript();
}

void ULuaCameraModifier::SetScriptPath(const FString& InScriptPath)
{
	if (ScriptPath == InScriptPath)
	{
		return;
	}

	ScriptPath = InScriptPath;
	bScriptLoaded = false;
	SetLastScriptError("");
}

bool ULuaCameraModifier::ReloadScript()
{
	UnloadScript();

	if (ScriptPath.empty())
	{
		SetLastScriptError("No Lua camera modifier script assigned.");
		return false;
	}

#if WITH_LUA
	const std::wstring AbsolutePath = FPaths::ToAbsolute(FPaths::ToWide(ScriptPath));
	if (!std::filesystem::exists(AbsolutePath))
	{
		SetLastScriptError("Lua camera modifier script does not exist: " + ScriptPath);
		UE_LOG("LuaCameraModifier: %s", LastScriptError.c_str());
		return false;
	}

	LuaState = std::make_unique<sol::state>();
	LuaState->open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
	RegisterLuaBindings(*LuaState);
	LuaState->set("CameraManager", CameraOwner);

	FString ScriptSource;
	FString ReadError;
	if (!LoadLuaSourceFromFile(ScriptPath, ScriptSource, ReadError))
	{
		SetLastScriptError(ReadError);
		UE_LOG("LuaCameraModifier: failed to load '%s': %s", ScriptPath.c_str(), LastScriptError.c_str());
		LuaState.reset();
		return false;
	}

	sol::protected_function_result Result = LuaState->safe_script(ScriptSource, sol::script_pass_on_error);
	if (!Result.valid())
	{
		sol::error Error = Result;
		SetLastScriptError(Error.what());
		UE_LOG("LuaCameraModifier: failed to load '%s': %s", ScriptPath.c_str(), LastScriptError.c_str());
		LuaState.reset();
		return false;
	}

	bScriptLoaded = true;
	SetLastScriptError("");
	return true;
#else
	SetLastScriptError("Lua runtime is disabled. Build with WITH_LUA=1.");
	return false;
#endif
}

void ULuaCameraModifier::UnloadScript()
{
#if WITH_LUA
	LuaState.reset();
#endif
	bScriptLoaded = false;
}

void ULuaCameraModifier::AddedToCamera(APlayerCameraManager* Camera)
{
	CameraOwner = Camera;
	if (!ScriptPath.empty())
	{
		ReloadScript();
	}
}

void ULuaCameraModifier::RemovedFromCamera(APlayerCameraManager* Camera)
{
	if (CameraOwner == Camera)
	{
		CameraOwner = nullptr;
	}
	UnloadScript();
}

bool ULuaCameraModifier::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
	return CallModifierFunction("ModifyCamera", DeltaTime, InOutView);
}

bool ULuaCameraModifier::ModifyPostProcess(float DeltaTime, FPostProcessSettings& InOutSettings)
{
	return CallModifierFunction("ModifyPostProcess", DeltaTime, InOutSettings);
}

bool ULuaCameraModifier::ModifyOverlay(float DeltaTime, FCameraOverlaySettings& InOutOverlay)
{
	return CallModifierFunction("ModifyOverlay", DeltaTime, InOutOverlay);
}

bool ULuaCameraModifier::CallModifierFunction(const char* FunctionName, float DeltaTime, FCameraViewInfo& InOutView)
{
#if WITH_LUA
	return bScriptLoaded && CallLuaModifierFunction(LuaState.get(), ScriptPath, FunctionName, DeltaTime, InOutView, LastScriptError);
#else
	(void)FunctionName;
	(void)DeltaTime;
	(void)InOutView;
	return false;
#endif
}

bool ULuaCameraModifier::CallModifierFunction(const char* FunctionName, float DeltaTime, FPostProcessSettings& InOutSettings)
{
#if WITH_LUA
	return bScriptLoaded && CallLuaModifierFunction(LuaState.get(), ScriptPath, FunctionName, DeltaTime, InOutSettings, LastScriptError);
#else
	(void)FunctionName;
	(void)DeltaTime;
	(void)InOutSettings;
	return false;
#endif
}

bool ULuaCameraModifier::CallModifierFunction(const char* FunctionName, float DeltaTime, FCameraOverlaySettings& InOutOverlay)
{
#if WITH_LUA
	return bScriptLoaded && CallLuaModifierFunction(LuaState.get(), ScriptPath, FunctionName, DeltaTime, InOutOverlay, LastScriptError);
#else
	(void)FunctionName;
	(void)DeltaTime;
	(void)InOutOverlay;
	return false;
#endif
}

void ULuaCameraModifier::SetLastScriptError(const FString& Error)
{
	LastScriptError = Error;
}
