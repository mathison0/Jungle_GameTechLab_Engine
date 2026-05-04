#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Collision/Collider2DComponent.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"
#include "Scripting/LuaScriptTypes.h"
#include "UI/ButtonComponent.h"

namespace
{
    UClass* FindClassByName(const FString& ClassName)
    {
        if (ClassName.empty()) return nullptr;
        for (UClass* Class : UClass::GetAllClasses())
        {
            if (Class && ClassName == Class->GetName()) return Class;
        }
        return nullptr;
    }

    bool DoesComponentMatchClass(UActorComponent* Component, const FString& ClassName)
    {
        if (!Component) return false;
        if (ClassName.empty()) return true;
        UClass* TargetClass = FindClassByName(ClassName);
        if (!TargetClass || !Component->GetClass()) return false;
        return Component->GetClass()->IsA(TargetClass);
    }
}

FLuaComponentHandle::FLuaComponentHandle(const UActorComponent* InComponent)
    : UUID(InComponent ? InComponent->GetUUID() : 0)
{
}

UActorComponent* FLuaComponentHandle::Resolve() const
{
    if (UUID == 0) return nullptr;
    UObject* Object = UObjectManager::Get().FindByUUID(UUID);
    return Cast<UActorComponent>(Object);
}

bool FLuaComponentHandle::IsValid() const
{
    return Resolve() != nullptr;
}

FString FLuaComponentHandle::GetName() const
{
    UActorComponent* Component = Resolve();
    return Component ? Component->GetFName().ToString() : "";
}

FString FLuaComponentHandle::GetComponentClassName() const
{
    UActorComponent* Component = Resolve();
    return Component && Component->GetClass() ? Component->GetClass()->GetName() : "";
}

bool FLuaComponentHandle::IsA(const FString& ClassName) const
{
    UActorComponent* Component = Resolve();
    return DoesComponentMatchClass(Component, ClassName);
}

FLuaActorHandle FLuaComponentHandle::GetOwner() const
{
    UActorComponent* Component = Resolve();
    return FLuaActorHandle(Component ? Component->GetOwner() : nullptr);
}

bool FLuaComponentHandle::IsActive() const
{
    UActorComponent* Component = Resolve();
    return Component ? Component->IsActive() : false;
}

bool FLuaComponentHandle::SetActive(bool bActive) const
{
    UActorComponent* Component = Resolve();
    if (!Component) return false;
    Component->SetActive(bActive);
    return true;
}

sol::table FLuaComponentHandle::GetWorldLocation(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetWorldLocation() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetWorldLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->SetWorldLocation(Location);
    return true;
}

sol::table FLuaComponentHandle::GetRelativeLocation(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetRelativeLocation() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetRelativeLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->SetRelativeLocation(Location);
    return true;
}

sol::table FLuaComponentHandle::GetForwardVector(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetForwardVector() : FVector(0.0f, 0.0f, 1.0f));
}

bool FLuaComponentHandle::LookAt(const sol::object& Value) const
{
    FVector Target;
    if (!ReadLuaVec3(Value, Target)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->LookAt(Target);
    return true;
}

sol::object FLuaComponentHandle::LuaCast(sol::this_state State, const FString& ClassName) const
{
    sol::state_view Lua(State);
    UActorComponent* Component = Resolve();
    if (!Component) return sol::nil;

    if (ClassName == "Collider2D" || ClassName == "UCollider2DComponent")
    {
        if (Cast<UCollider2DComponent>(Component)) return sol::make_object(Lua, FLuaCollider2DHandle(Component));
        return sol::nil;
    }
    if (ClassName == "BoxCollider2D" || ClassName == "UBoxCollider2DComponent")
    {
        if (IsA("UBoxCollider2DComponent")) return sol::make_object(Lua, FLuaBoxCollider2DHandle(Component));
        return sol::nil;
    }
    if (ClassName == "CircleCollider2D" || ClassName == "UCircleCollider2DComponent")
    {
        if (IsA("UCircleCollider2DComponent")) return sol::make_object(Lua, FLuaCircleCollider2DHandle(Component));
        return sol::nil;
    }
    UE_LOG("[Lua]", Warning, "LuaCast to %s not supported.", ClassName.c_str());
    return sol::nil;
}

bool FLuaComponentHandle::IsVisible() const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    return PrimitiveComponent ? PrimitiveComponent->IsVisible() : false;
}

bool FLuaComponentHandle::SetVisibility(bool bVisible) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    if (!PrimitiveComponent) return false;
    PrimitiveComponent->SetVisibility(bVisible);
    return true;
}

bool FLuaComponentHandle::ShouldGenerateOverlapEvents() const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    return PrimitiveComponent ? PrimitiveComponent->ShouldGenerateOverlapEvents() : false;
}

bool FLuaComponentHandle::SetGenerateOverlapEvents(bool bGenerate) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    if (!PrimitiveComponent) return false;
    PrimitiveComponent->SetGenerateOverlapEvents(bGenerate);
    return true;
}

bool FLuaComponentHandle::IsOverlappingActor(const FLuaActorHandle& OtherActor) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    AActor* Other = OtherActor.Resolve();
    if (!PrimitiveComponent || !Other) return false;
    return PrimitiveComponent->IsOverlappingActor(Other);
}

bool FLuaComponentHandle::IsOverlappingComponent(const FLuaComponentHandle& OtherComponent) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    UPrimitiveComponent* OtherPrimitive = Cast<UPrimitiveComponent>(OtherComponent.Resolve());
    if (!PrimitiveComponent || !OtherPrimitive) return false;
    return PrimitiveComponent->IsOverlappingComponent(OtherPrimitive);
}

bool FLuaComponentHandle::IsUIButton() const
{
    return Cast<UUIButtonComponent>(Resolve()) != nullptr;
}

bool FLuaComponentHandle::IsButtonInteractable() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->IsInteractable() : false;
}

bool FLuaComponentHandle::SetButtonInteractable(bool bInteractable) const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    if (!Button) return false;
    Button->SetInteractable(bInteractable);
    return true;
}

bool FLuaComponentHandle::IsButtonHovered() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->IsHovered() : false;
}

bool FLuaComponentHandle::IsButtonPressed() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->IsPressed() : false;
}

int32 FLuaComponentHandle::GetButtonClickCount() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->GetClickCount() : 0;
}

namespace LuaBinding
{
    void RegisterComponent(sol::state& Lua)
    {
        Lua.new_usertype<FLuaComponentHandle>(
            "Component",
            sol::no_constructor,

            "IsValid", &FLuaComponentHandle::IsValid,
            "GetName", &FLuaComponentHandle::GetName,
            "GetClassName", &FLuaComponentHandle::GetComponentClassName,
            "IsA", &FLuaComponentHandle::IsA,
            "GetOwner", &FLuaComponentHandle::GetOwner,
            "IsActive", &FLuaComponentHandle::IsActive,
            "SetActive", &FLuaComponentHandle::SetActive,

            "GetWorldLocation", &FLuaComponentHandle::GetWorldLocation,
            "SetWorldLocation", &FLuaComponentHandle::SetWorldLocation,
            "GetRelativeLocation", &FLuaComponentHandle::GetRelativeLocation,
            "SetRelativeLocation", &FLuaComponentHandle::SetRelativeLocation,
            "GetForwardVector", &FLuaComponentHandle::GetForwardVector,
            "LookAt", &FLuaComponentHandle::LookAt,
            "Cast", &FLuaComponentHandle::LuaCast,

            "IsVisible", &FLuaComponentHandle::IsVisible,
            "SetVisibility", &FLuaComponentHandle::SetVisibility,
            "ShouldGenerateOverlapEvents", &FLuaComponentHandle::ShouldGenerateOverlapEvents,
            "SetGenerateOverlapEvents", &FLuaComponentHandle::SetGenerateOverlapEvents,
            "IsOverlappingActor", &FLuaComponentHandle::IsOverlappingActor,
            "IsOverlappingComponent", &FLuaComponentHandle::IsOverlappingComponent,

            "IsUIButton", &FLuaComponentHandle::IsUIButton,
            "IsButtonInteractable", &FLuaComponentHandle::IsButtonInteractable,
            "SetButtonInteractable", &FLuaComponentHandle::SetButtonInteractable,
            "IsButtonHovered", &FLuaComponentHandle::IsButtonHovered,
            "IsButtonPressed", &FLuaComponentHandle::IsButtonPressed,
            "GetButtonClickCount", &FLuaComponentHandle::GetButtonClickCount);
    }
}
