#include "ScriptComponent.h"

#include "Core/Logging/LogMacros.h"
#include "Object/ObjectFactory.h"
#include "Runtime/Engine.h"
#include "Serialization/Archive.h"

#include <cstring>

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

void UScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

    if (LoadScript())
    {
        BindFunctions();
        CallLuaFunction("BeginPlay");
    }
}

void UScriptComponent::EndPlay()
{
    CallLuaFunction("EndPlay");
    ScriptInstance = sol::nil;

    UActorComponent::EndPlay();
}

void UScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    CallLuaTick(DeltaTime);

    CoroutineExecutorSet.Tick({ DeltaTime });
}

void UScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << ScriptPath;
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ .Name = "Script",
                         .Type = EPropertyType::String,
                         .ValuePtr = &ScriptPath });
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Script") == 0)
    {
        LoadScript();
    }
}

bool UScriptComponent::LoadScript()
{
    ScriptInstance = sol::nil;

    if (ScriptPath.empty() || !GEngine)
    {
        return false;
    }

    ScriptInstance = GEngine->GetScriptSystem().CreateScriptInstance(ScriptPath);
    if (!ScriptInstance.valid())
    {
        UE_LOG([Lua], Warning, "Failed to create Lua script instance: %s", ScriptPath.c_str());
        return false;
    }

    return true;
}

void UScriptComponent::CallLuaFunction(const char* Name)
{
    if (!ScriptInstance.valid())
    {
        return;
    }

    sol::object FunctionObject = ScriptInstance[Name];
    if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
    {
        return;
    }

    sol::protected_function Function = FunctionObject;
    sol::protected_function_result Result = Function(ScriptInstance);
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG([Lua], Error, "Lua script function '%s' failed in '%s': %s",
               Name, ScriptPath.c_str(), Err.what());
    }
}

void UScriptComponent::CallLuaTick(float DeltaTime)
{
    if (!ScriptInstance.valid())
    {
        return;
    }

    sol::object FunctionObject = ScriptInstance["Tick"];
    if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
    {
        return;
    }

    sol::protected_function Function = FunctionObject;
    sol::protected_function_result Result = Function(ScriptInstance, DeltaTime);
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG([Lua], Error, "Lua script function 'Tick' failed in '%s': %s",
               ScriptPath.c_str(), Err.what());
    }
}

void UScriptComponent::BindFunctions()
{
    BindFunction("start_coroutine",
                 [this](sol::function Func) -> uint32
                 { return CoroutineExecutorSet.Start(Func); });

    BindFunction("stop_coroutine",
                 [this](uint32 FuncKey) -> bool
                 { return CoroutineExecutorSet.Stop(FuncKey); });

	BindFunction("get_location",
                 [this]()
                 { return Owner->GetActorLocation(); });

	BindFunction("get_rotation",
                 [this]()
                 { return Owner->GetActorRotation(); });
}
