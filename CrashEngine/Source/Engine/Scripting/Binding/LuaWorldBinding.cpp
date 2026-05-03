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

sol::table FLuaWorldHandle::OverlapCircle(sol::this_state State, const sol::object& Position, float Radius, const FString& Tag) const
{
    sol::state_view Lua(State);
    sol::table Result = Lua.create_table();
    if (!World || Radius < 0.0f) return Result;
    FVector Pos;
    if (!ReadLuaVec3(Position, Pos)) return Result;
    TArray<UCollider2DComponent*> Colliders;
    World->OverlapCircle(FVector2(Pos.X, Pos.Y), Radius, Colliders);
    FName TargetTag(Tag);
    std::set<AActor*> UniqueActors;
    for (UCollider2DComponent* Collider : Colliders)
    {
        if (Collider)
        {
            AActor* Actor = Collider->GetOwner();
            if (Actor && (Tag.empty() || Actor->GetActorTag() == TargetTag)) UniqueActors.insert(Actor);
        }
    }
    int32 Index = 1;
    for (AActor* Actor : UniqueActors) Result[Index++] = FLuaActorHandle(Actor);
    return Result;
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
            "OverlapCircle", &FLuaWorldHandle::OverlapCircle,
            "GetActorsByTag", &FLuaWorldHandle::GetActorsByTag);
    }
}
