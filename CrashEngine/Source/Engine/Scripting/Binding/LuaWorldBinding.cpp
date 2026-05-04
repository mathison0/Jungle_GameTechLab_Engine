#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/Collision/Collider2DComponent.h"
#include "Scripting/LuaScriptTypes.h"
#include <set>

FLuaWorldHandle::FLuaWorldHandle(UWorld* InWorld)
    : World(InWorld)
{
}

bool FLuaWorldHandle::IsValid() const
{
    return World != nullptr;
}

bool FLuaWorldHandle::IsGameplayPaused() const
{
    return World ? World->IsGameplayPaused() : false;
}

void FLuaWorldHandle::SetGameplayPaused(bool bPaused) const
{
    if (World)
    {
        World->SetGameplayPaused(bPaused);
    }
}

sol::table FLuaWorldHandle::GetActorsByTag(sol::this_state State, const FString& Tag) const
{
    sol::state_view Lua(State);
    sol::table Result = Lua.create_table();
    if (!World) return Result;
    FName TargetTag(Tag);
    int32 Index = 1;
    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->GetActorTag() == TargetTag) Result[Index++] = FLuaActorHandle(Actor);
    }
    return Result;
}

namespace LuaBinding
{
    void RegisterWorld(sol::state& Lua)
    {
        Lua.new_usertype<FLuaWorldHandle>(
            "World",
            sol::no_constructor,
            "IsValid", &FLuaWorldHandle::IsValid,
            "IsGameplayPaused", &FLuaWorldHandle::IsGameplayPaused,
            "SetGameplayPaused", &FLuaWorldHandle::SetGameplayPaused,
            "GetActorsByTag", &FLuaWorldHandle::GetActorsByTag);
    }
}
