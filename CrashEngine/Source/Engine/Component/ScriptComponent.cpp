#include "ScriptComponent.h"

#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Runtime/Engine.h"
#include "Serialization/Archive.h"
#include "Scripting/LuaScriptAsset.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

namespace
{
constexpr const char* LuaPropertyPrefix = "Lua.";

FString MakeLuaPropertyIdentifier(const FString& Name)
{
    return FString(LuaPropertyPrefix) + Name;
}

bool IsLuaPropertyIdentifier(const char* Name)
{
    return Name && std::strncmp(Name, LuaPropertyPrefix, std::strlen(LuaPropertyPrefix)) == 0;
}

FLuaScriptPropertyOverride* FindScriptPropertyOverride(TArray<FLuaScriptPropertyOverride>& Properties, const FString& Name)
{
    auto It = std::find_if(Properties.begin(), Properties.end(),
                           [&Name](const FLuaScriptPropertyOverride& Property)
                           {
                               return Property.Name == Name;
                           });
    return It != Properties.end() ? &(*It) : nullptr;
}

bool IsScriptInstanceArgument(const sol::object& Arg)
{
    if (!Arg.is<sol::table>())
    {
        return false;
    }

    sol::table Table = Arg.as<sol::table>();
    sol::object AssetPath = Table["__assetPath"];
    sol::object AssetVersion = Table["__assetVersion"];
    return AssetPath.valid() && AssetPath != sol::nil &&
           AssetVersion.valid() && AssetVersion != sol::nil;
}

bool ReadFirstVec3Argument(sol::variadic_args Args, FVector& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (IsScriptInstanceArgument(Arg))
        {
            continue;
        }

        if (ReadLuaVec3(Arg, OutValue))
        {
            return true;
        }
    }
    return false;
}

bool ReadFirstBoolArgument(sol::variadic_args Args, bool& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::boolean)
        {
            OutValue = Arg.as<bool>();
            return true;
        }
    }
    return false;
}

bool ReadFirstFunctionArgument(sol::variadic_args Args, sol::function& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::function)
        {
            OutValue = Arg.as<sol::function>();
            return true;
        }
    }
    return false;
}

bool ReadFirstUIntArgument(sol::variadic_args Args, uint32& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::number)
        {
            OutValue = Arg.as<uint32>();
            return true;
        }
    }
    return false;
}
} // namespace

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
    Ar << ScriptProperties;
    if (Ar.IsLoading())
    {
        SyncedScriptPath.clear();
        SyncedScriptVersion = 0;
    }
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ .Name = "Script",
                         .Type = EPropertyType::String,
                         .ValuePtr = &ScriptPath });

    SyncScriptPropertiesWithAsset();

    if (!GEngine || ScriptPath.empty())
    {
        return;
    }

    FLuaScriptAsset* Asset = GEngine->GetScriptSystem().GetScriptAsset(ScriptPath);
    if (!Asset || !Asset->IsUsable())
    {
        return;
    }

    const TArray<FLuaScriptPropertyDesc>& Descriptors = Asset->GetPropertyDescriptors();
    for (const FLuaScriptPropertyDesc& Desc : Descriptors)
    {
        FLuaScriptPropertyOverride* Override = FindScriptPropertyOverride(ScriptProperties, Desc.Name);
        if (!Override)
        {
            continue;
        }

        void* ValuePtr = GetLuaScriptValuePtr(Override->Value);
        if (!ValuePtr)
        {
            continue;
        }

        OutProps.push_back({ .Name = MakeLuaPropertyIdentifier(Desc.Name),
                             .Type = GetLuaScriptEditorPropertyType(Desc.Type),
                             .ValuePtr = ValuePtr,
                             .Min = Desc.Min,
                             .Max = Desc.Max,
                             .Speed = Desc.Speed });
    }
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Script") == 0)
    {
        SetScriptPath(ScriptPath);
    }
    else if (IsLuaPropertyIdentifier(PropertyName))
    {
        // Values are applied when the Lua instance is created at BeginPlay.
    }
}

void UScriptComponent::SetScriptPath(const FString& InScriptPath)
{
    ScriptPath = InScriptPath;
    SyncedScriptPath.clear();
    SyncedScriptVersion = 0;
    SyncScriptPropertiesWithAsset();
}

bool UScriptComponent::LoadScript()
{
    ScriptInstance = sol::nil;

    if (ScriptPath.empty() || !GEngine)
    {
        return false;
    }

    SyncScriptPropertiesWithAsset();
    ScriptInstance = GEngine->GetScriptSystem().CreateScriptInstance(ScriptPath, &ScriptProperties);
    if (!ScriptInstance.valid())
    {
        UE_LOG([Lua], Warning, "Failed to create Lua script instance: %s", ScriptPath.c_str());
        return false;
    }

    return true;
}

void UScriptComponent::SyncScriptPropertiesWithAsset()
{
    if (ScriptPath.empty() || !GEngine)
    {
        ScriptProperties.clear();
        SyncedScriptPath.clear();
        SyncedScriptVersion = 0;
        return;
    }

    FLuaScriptAsset* Asset = GEngine->GetScriptSystem().GetScriptAsset(ScriptPath);
    if (!Asset || !Asset->IsUsable())
    {
        if (SyncedScriptPath != ScriptPath)
        {
            ScriptProperties.clear();
            SyncedScriptPath = ScriptPath;
            SyncedScriptVersion = 0;
        }
        return;
    }

    if (SyncedScriptPath == ScriptPath && SyncedScriptVersion == Asset->GetVersion())
    {
        return;
    }

    const TArray<FLuaScriptPropertyDesc>& Descriptors = Asset->GetPropertyDescriptors();
    TArray<FLuaScriptPropertyOverride> NewProperties;
    NewProperties.reserve(Descriptors.size());

    for (const FLuaScriptPropertyDesc& Desc : Descriptors)
    {
        FLuaScriptPropertyOverride NewProperty;
        NewProperty.Name = Desc.Name;

        FLuaScriptPropertyOverride* ExistingProperty = FindScriptPropertyOverride(ScriptProperties, Desc.Name);
        if (ExistingProperty && ExistingProperty->Value.Type == Desc.Type)
        {
            NewProperty.Value = ExistingProperty->Value;
        }
        else
        {
            NewProperty.Value = Desc.DefaultValue;
        }
        NewProperty.Value.Type = Desc.Type;

        NewProperties.push_back(std::move(NewProperty));
    }

    ScriptProperties = std::move(NewProperties);
    SyncedScriptPath = ScriptPath;
    SyncedScriptVersion = Asset->GetVersion();
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
                 [this](const sol::variadic_args& Args) -> uint32
                 {
                     sol::function Func;
                     if (!ReadFirstFunctionArgument(Args, Func))
                     {
                         UE_LOG([Lua], Warning, "start_coroutine expects a function in '%s'", ScriptPath.c_str());
                         return 0;
                     }
                     return CoroutineExecutorSet.Start(Func);
                 });

    BindFunction("stop_coroutine",
                 [this](const sol::variadic_args& Args) -> bool
                 {
                     uint32 FuncKey = 0;
                     if (!ReadFirstUIntArgument(Args, FuncKey))
                     {
                         UE_LOG([Lua], Warning, "stop_coroutine expects a coroutine id in '%s'", ScriptPath.c_str());
                         return false;
                     }
                     return CoroutineExecutorSet.Stop(FuncKey);
                 });

    BindFunction("GetActorLocation",
                 [this](sol::variadic_args, sol::this_state State) -> sol::table
                 {
                     sol::state_view Lua(State);
                     AActor* Owner = GetOwner();
                     return MakeLuaVec3(Lua, Owner ? Owner->GetActorLocation() : FVector(0.0f, 0.0f, 0.0f));
                 });

    BindFunction("SetActorLocation",
                 [this](const sol::variadic_args& Args) -> bool
                 {
                     FVector Location;
                     if (!ReadFirstVec3Argument(Args, Location))
                     {
                         UE_LOG([Lua], Warning, "SetActorLocation expects a vec3 table in '%s'", ScriptPath.c_str());
                         return false;
                     }

                     AActor* Owner = GetOwner();
                     if (!Owner)
                     {
                         return false;
                     }

                     Owner->SetActorLocation(Location);
                     return true;
                 });

    BindFunction("AddActorWorldOffset",
                 [this](const sol::variadic_args& Args) -> bool
                 {
                     FVector Delta;
                     if (!ReadFirstVec3Argument(Args, Delta))
                     {
                         UE_LOG([Lua], Warning, "AddActorWorldOffset expects a vec3 table in '%s'", ScriptPath.c_str());
                         return false;
                     }

                     AActor* Owner = GetOwner();
                     if (!Owner)
                     {
                         return false;
                     }

                     Owner->AddActorWorldOffset(Delta);
                     return true;
                 });

    BindFunction("GetActorRotation",
                 [this](sol::variadic_args, sol::this_state State) -> sol::table
                 {
                     sol::state_view Lua(State);
                     AActor* Owner = GetOwner();
                     return MakeLuaVec3(Lua, Owner ? Owner->GetActorRotation().ToVector() : FVector(0.0f, 0.0f, 0.0f));
                 });

    BindFunction("SetActorRotation",
                 [this](const sol::variadic_args& Args) -> bool
                 {
                     FVector Rotation;
                     if (!ReadFirstVec3Argument(Args, Rotation))
                     {
                         UE_LOG([Lua], Warning, "SetActorRotation expects a vec3 table in '%s'", ScriptPath.c_str());
                         return false;
                     }

                     AActor* Owner = GetOwner();
                     if (!Owner)
                     {
                         return false;
                     }

                     Owner->SetActorRotation(Rotation);
                     return true;
                 });

    BindFunction("GetActorScale",
                 [this](sol::variadic_args, sol::this_state State) -> sol::table
                 {
                     sol::state_view Lua(State);
                     AActor* Owner = GetOwner();
                     return MakeLuaVec3(Lua, Owner ? Owner->GetActorScale() : FVector(1.0f, 1.0f, 1.0f));
                 });

    BindFunction("SetActorScale",
                 [this](const sol::variadic_args& Args) -> bool
                 {
                     FVector Scale;
                     if (!ReadFirstVec3Argument(Args, Scale))
                     {
                         UE_LOG([Lua], Warning, "SetActorScale expects a vec3 table in '%s'", ScriptPath.c_str());
                         return false;
                     }

                     AActor* Owner = GetOwner();
                     if (!Owner)
                     {
                         return false;
                     }

                     Owner->SetActorScale(Scale);
                     return true;
                 });

    BindFunction("GetActorForward",
                 [this](sol::variadic_args, sol::this_state State) -> sol::table
                 {
                     sol::state_view Lua(State);
                     AActor* Owner = GetOwner();
                     return MakeLuaVec3(Lua, Owner ? Owner->GetActorForward() : FVector(0.0f, 0.0f, 1.0f));
                 });

    BindFunction("IsActorVisible",
                 [this](sol::variadic_args) -> bool
                 {
                     AActor* Owner = GetOwner();
                     return Owner ? Owner->IsVisible() : false;
                 });

    BindFunction("SetActorVisible",
                 [this](const sol::variadic_args& Args) -> bool
                 {
                     bool bVisible = false;
                     if (!ReadFirstBoolArgument(Args, bVisible))
                     {
                         UE_LOG([Lua], Warning, "SetActorVisible expects a bool in '%s'", ScriptPath.c_str());
                         return false;
                     }

                     AActor* Owner = GetOwner();
                     if (!Owner)
                     {
                         return false;
                     }

                     Owner->SetVisible(bVisible);
                     return true;
                 });
}
