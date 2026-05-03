#include "LuaEngineBinding.h"

#include <algorithm>

#include "Component/PrimitiveComponent.h"
#include "Component/Collision/BoxCollider2DComponent.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialCore.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Scripting/LuaScriptTypes.h"

namespace
{
UClass* FindClassByName(const FString& ClassName)
{
    if (ClassName.empty())
    {
        return nullptr;
    }

    for (UClass* Class : UClass::GetAllClasses())
    {
        if (Class && ClassName == Class->GetName())
        {
            return Class;
        }
    }

    return nullptr;
}

bool DoesComponentMatchClass(UActorComponent* Component, const FString& ClassName)
{
    if (!Component)
    {
        return false;
    }

    if (ClassName.empty())
    {
        return true;
    }

    UClass* TargetClass = FindClassByName(ClassName);
    if (!TargetClass || !Component->GetClass())
    {
        return false;
    }

    return Component->GetClass()->IsA(TargetClass);
}
} // namespace

FLuaActorHandle::FLuaActorHandle(const AActor* InActor)
    : UUID(InActor ? InActor->GetUUID() : 0)
{
}

AActor* FLuaActorHandle::Resolve() const
{
    if (UUID == 0)
    {
        return nullptr;
    }

    UObject* Object = UObjectManager::Get().FindByUUID(UUID);
    return Cast<AActor>(Object);
}

bool FLuaActorHandle::IsValid() const
{
    return Resolve() != nullptr;
}

FString FLuaActorHandle::GetName() const
{
    AActor* Actor = Resolve();
    return Actor ? Actor->GetFName().ToString() : "";
}

FString FLuaActorHandle::GetActorClassName() const
{
    AActor* Actor = Resolve();
    return Actor && Actor->GetClass() ? Actor->GetClass()->GetName() : "";
}

sol::table FLuaActorHandle::GetLocation(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();

    return MakeLuaVec3(
        Lua,
        Actor ? Actor->GetActorLocation() : FVector(0.f, 0.f, 0.f));
}

bool FLuaActorHandle::SetLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location))
    {
        return false;
    }

    AActor* Actor = Resolve();
    if (!Actor)
    {
        return false;
    }

    Actor->SetActorLocation(Location);
    return true;
}

bool FLuaActorHandle::AddWorldOffset(const sol::object& Value) const
{
    FVector Delta;
    if (!ReadLuaVec3(Value, Delta))
    {
        return false;
    }

    AActor* Actor = Resolve();
    if (!Actor)
    {
        return false;
    }

    Actor->AddActorWorldOffset(Delta);
    return true;
}

sol::table FLuaActorHandle::GetRotation(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();

    return MakeLuaVec3(
        Lua,
        Actor ? Actor->GetActorRotation().ToVector() : FVector(0.f, 0.f, 0.f));
}

bool FLuaActorHandle::SetRotation(const sol::object& Value) const
{
    FVector Rotation;
    if (!ReadLuaVec3(Value, Rotation))
    {
        return false;
    }

    AActor* Actor = Resolve();
    if (!Actor)
    {
        return false;
    }

    Actor->SetActorRotation(Rotation);
    return true;
}

sol::table FLuaActorHandle::GetScale(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();

    return MakeLuaVec3(
        Lua,
        Actor ? Actor->GetActorScale() : FVector(1.0f, 1.0f, 1.0f));
}

bool FLuaActorHandle::SetScale(const sol::object& Value) const
{
    FVector Scale;
    if (!ReadLuaVec3(Value, Scale))
    {
        return false;
    }

    AActor* Actor = Resolve();
    if (!Actor)
    {
        return false;
    }

    Actor->SetActorScale(Scale);
    return true;
}

sol::table FLuaActorHandle::GetForward(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();

    return MakeLuaVec3(
        Lua,
        Actor ? Actor->GetActorForward() : FVector(0.0f, 0.0f, 1.0f));
}

bool FLuaActorHandle::IsVisible() const
{
    AActor* Actor = Resolve();
    return Actor ? Actor->IsVisible() : false;
}

bool FLuaActorHandle::SetVisible(bool bVisible) const
{
    AActor* Actor = Resolve();
    if (!Actor)
    {
        return false;
    }

    Actor->SetVisible(bVisible);
    return true;
}

FLuaComponentHandle FLuaActorHandle::GetComponent(const sol::variadic_args& Args) const
{
    FString ClassName;
    int32 TargetIndex = 1;

    for (const sol::object& Arg : Args)
    {
        if (Arg.is<FString>())
        {
            ClassName = Arg.as<FString>();
        }
        else if (Arg.get_type() == sol::type::number)
        {
            TargetIndex = Arg.as<int32>();
        }
    }

    TargetIndex = std::max(TargetIndex, 1);

    AActor* Actor = Resolve();
    if (!Actor)
    {
        return FLuaComponentHandle();
    }

    int32 MatchIndex = 1;
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (!DoesComponentMatchClass(Component, ClassName))
        {
            continue;
        }

        if (MatchIndex == TargetIndex)
        {
            return FLuaComponentHandle(Component);
        }

        ++MatchIndex;
    }

    return FLuaComponentHandle();
}

sol::table FLuaActorHandle::GetComponents(sol::this_state State, const sol::variadic_args& Args) const
{
    sol::state_view Lua(State);
    sol::table Result = Lua.create_table();

    FString ClassName;
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<FString>())
        {
            ClassName = Arg.as<FString>();
            break;
        }
    }

    AActor* Actor = Resolve();
    if (!Actor)
    {
        return Result;
    }

    int32 LuaIndex = 1;
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (DoesComponentMatchClass(Component, ClassName))
        {
            Result[LuaIndex++] = FLuaComponentHandle(Component);
        }
    }

    return Result;
}

FLuaComponentHandle::FLuaComponentHandle(const UActorComponent* InComponent)
    : UUID(InComponent ? InComponent->GetUUID() : 0)
{
}

UActorComponent* FLuaComponentHandle::Resolve() const
{
    if (UUID == 0)
    {
        return nullptr;
    }

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
    if (!Component)
    {
        return false;
    }

    Component->SetActive(bActive);
    return true;
}

sol::table FLuaComponentHandle::GetWorldLocation(sol::this_state State) const
{
    sol::state_view Lua(State);

    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(
        Lua,
        SceneComponent ? SceneComponent->GetWorldLocation() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetWorldLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location))
    {
        return false;
    }

    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent)
    {
        return false;
    }

    SceneComponent->SetWorldLocation(Location);
    return true;
}

sol::table FLuaComponentHandle::GetRelativeLocation(sol::this_state State) const
{
    sol::state_view Lua(State);

    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(
        Lua,
        SceneComponent ? SceneComponent->GetRelativeLocation() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetRelativeLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location))
    {
        return false;
    }

    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent)
    {
        return false;
    }

    SceneComponent->SetRelativeLocation(Location);
    return true;
}

sol::table FLuaComponentHandle::GetForwardVector(sol::this_state State) const
{
    sol::state_view Lua(State);

    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(
        Lua,
        SceneComponent ? SceneComponent->GetForwardVector() : FVector(0.0f, 0.0f, 1.0f));
}

bool FLuaComponentHandle::LookAt(const sol::object& Value) const
{
    FVector Target;
    if (!ReadLuaVec3(Value, Target))
    {
        return false;
    }

    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent)
    {
        return false;
    }

    SceneComponent->LookAt(Target);
    return true;
}

sol::object FLuaComponentHandle::LuaCast(sol::this_state State, const FString& ClassName) const
{
    sol::state_view Lua(State);
    UActorComponent* Component = Resolve();
    if (!Component)
    {
        return sol::nil;
    }

    // Collider2D로 Cast
    if (ClassName == "Collider2D" || ClassName == "UCollider2DComponent")
    {
        if (::Cast<UCollider2DComponent>(Component))
        {
            return sol::make_object(Lua, FLuaCollider2DHandle(Component));
        }
        return sol::nil;
    }

    // 구체적인 콜라이더 타입으로 Cast (Box, Circle 등)
    if (ClassName == "BoxCollider2D" || ClassName == "UBoxCollider2DComponent")
    {
        if (IsA("UBoxCollider2DComponent"))
        {
            return sol::make_object(Lua, FLuaBoxCollider2DHandle(Component));
        }
        return sol::nil;
    }
    if (ClassName == "CircleCollider2D" || ClassName == "UCircleCollider2DComponent")
    {
        if (IsA("UCircleCollider2DComponent"))
        {
            return sol::make_object(Lua, FLuaCircleCollider2DHandle(Component));
        }
        return sol::nil;
    }

    UE_LOG("[Lua]", Warning, "LuaCast to %s not supported. See FLuaComponentHandle::LuaCast() definition", ClassName.c_str());
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
    if (!PrimitiveComponent)
    {
        return false;
    }

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
    if (!PrimitiveComponent)
    {
        return false;
    }

    PrimitiveComponent->SetGenerateOverlapEvents(bGenerate);
    return true;
}

bool FLuaComponentHandle::IsOverlappingActor(const FLuaActorHandle& OtherActor) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    AActor* Other = OtherActor.Resolve();

    if (!PrimitiveComponent || !Other)
    {
        return false;
    }

    return PrimitiveComponent->IsOverlappingActor(Other);
}

bool FLuaComponentHandle::IsOverlappingComponent(const FLuaComponentHandle& OtherComponent) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    UPrimitiveComponent* OtherPrimitive = Cast<UPrimitiveComponent>(OtherComponent.Resolve());

    if (!PrimitiveComponent || !OtherPrimitive)
    {
        return false;
    }

    return PrimitiveComponent->IsOverlappingComponent(OtherPrimitive);
}

UCollider2DComponent* FLuaCollider2DHandle::ResolveCollider() const
{
    return Cast<UCollider2DComponent>(Resolve());
}

sol::table FLuaCollider2DHandle::GetShapeWorldLocation2D(sol::this_state State) const
{
    sol::state_view Lua(State);
    UCollider2DComponent* Collider = ResolveCollider();
    return MakeLuaVec2(Lua, Collider ? Collider->GetShapeWorldLocation2D() : FVector2(0.f, 0.f));
}

float FLuaCollider2DHandle::GetCollisionPlaneZ() const
{
    UCollider2DComponent* Collider = ResolveCollider();
    return Collider ? Collider->GetCollisionPlaneZ() : 0.f;
}

sol::table FLuaBoxCollider2DHandle::GetBoxExtent(sol::this_state State) const
{
    sol::state_view Lua(State);
    UBoxCollider2DComponent* Box = Cast<UBoxCollider2DComponent>(Resolve());
    return MakeLuaVec2(Lua, Box ? Box->GetBoxExtent2D() : FVector2(0.f, 0.f));
}

bool FLuaBoxCollider2DHandle::SetBoxExtent(const sol::object& Value) const
{
    FVector2 Extent;
    if (!ReadLuaVec2(Value, Extent))
    {
        return false;
    }

    UBoxCollider2DComponent* Box = Cast<UBoxCollider2DComponent>(Resolve());
    if (!Box)
    {
        return false;
    }

    Box->SetBoxExtent2D(Extent);
    return true;
}

float FLuaCircleCollider2DHandle::GetRadius() const
{
    UCircleCollider2DComponent* Circle = Cast<UCircleCollider2DComponent>(Resolve());
    return Circle ? Circle->GetRadius() : 0.f;
}

bool FLuaCircleCollider2DHandle::SetRadius(float Radius) const
{
    UCircleCollider2DComponent* Circle = Cast<UCircleCollider2DComponent>(Resolve());
    if (!Circle)
    {
        return false;
    }

    Circle->SetRadius(Radius);
    return true;
}

void RegisterLuaEngineBindings(sol::state& Lua)
{
    Lua.new_usertype<FVector>(
        "FVector",
        sol::constructors<FVector(), FVector(float, float, float)>(),

        "X", &FVector::X,
        "Y", &FVector::Y,
        "Z", &FVector::Z,

        sol::meta_function::addition, [](const FVector& A, const FVector& B)
        { return A + B; },
        sol::meta_function::subtraction, [](const FVector& A, const FVector& B)
        { return A - B; },
        sol::meta_function::multiplication, sol::overload([](const FVector& V, float Scalar)
                                                          { return V * Scalar; }, [](float Scalar, const FVector& V)
                                                          { return V * Scalar; }));

    Lua.new_usertype<FVector2>(
        "FVector2",
        sol::constructors<FVector2(), FVector2(float, float)>(),

        "X", &FVector2::X,
        "Y", &FVector2::Y,

        sol::meta_function::addition, [](const FVector2& A, const FVector2& B)
        { return A + B; },
        sol::meta_function::subtraction, [](const FVector2& A, const FVector2& B)
        { return A - B; },
        sol::meta_function::multiplication, sol::overload([](const FVector2& V, float Scalar)
                                                          { return V * Scalar; }, [](float Scalar, const FVector2& V)
                                                          { return V * Scalar; }));

    Lua.new_usertype<FLuaActorHandle>(
        "Actor",
        sol::no_constructor,

        "IsValid", &FLuaActorHandle::IsValid,
        "GetName", &FLuaActorHandle::GetName,
        "GetClassName", &FLuaActorHandle::GetActorClassName,

        "GetLocation", &FLuaActorHandle::GetLocation,
        "SetLocation", &FLuaActorHandle::SetLocation,
        "AddWorldOffset", &FLuaActorHandle::AddWorldOffset,

        "GetRotation", &FLuaActorHandle::GetRotation,
        "SetRotation", &FLuaActorHandle::SetRotation,

        "GetScale", &FLuaActorHandle::GetScale,
        "SetScale", &FLuaActorHandle::SetScale,

        "GetForward", &FLuaActorHandle::GetForward,

        "IsVisible", &FLuaActorHandle::IsVisible,
        "SetVisible", &FLuaActorHandle::SetVisible,

        "GetComponent", &FLuaActorHandle::GetComponent,
        "GetComponents", &FLuaActorHandle::GetComponents);

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

        // SceneComponent API
        "GetWorldLocation", &FLuaComponentHandle::GetWorldLocation,
        "SetWorldLocation", &FLuaComponentHandle::SetWorldLocation,
        "GetRelativeLocation", &FLuaComponentHandle::GetRelativeLocation,
        "SetRelativeLocation", &FLuaComponentHandle::SetRelativeLocation,
        "GetForwardVector", &FLuaComponentHandle::GetForwardVector,
        "LookAt", &FLuaComponentHandle::LookAt,
        "Cast", &FLuaComponentHandle::LuaCast,

        // PrimitiveComponent API
        "IsVisible", &FLuaComponentHandle::IsVisible,
        "SetVisibility", &FLuaComponentHandle::SetVisibility,
        "ShouldGenerateOverlapEvents", &FLuaComponentHandle::ShouldGenerateOverlapEvents,
        "SetGenerateOverlapEvents", &FLuaComponentHandle::SetGenerateOverlapEvents,
        "IsOverlappingActor", &FLuaComponentHandle::IsOverlappingActor,
        "IsOverlappingComponent", &FLuaComponentHandle::IsOverlappingComponent);

    // ColliderComponent
    Lua.new_usertype<FLuaCollider2DHandle>(
        "Collider2D",
        sol::no_constructor,
        sol::base_classes, sol::bases<FLuaComponentHandle>(),

        "GetShapeWorldLocation2D", &FLuaCollider2DHandle::GetShapeWorldLocation2D,
        "GetCollisionPlaneZ", &FLuaCollider2DHandle::GetCollisionPlaneZ);
    
    Lua.new_usertype<FLuaBoxCollider2DHandle>(
        "BoxCollider2D",
        sol::no_constructor,
        sol::base_classes, sol::bases<FLuaCollider2DHandle>(),

        "GetBoxExtent", &FLuaBoxCollider2DHandle::GetBoxExtent,
        "SetBoxExtent", &FLuaBoxCollider2DHandle::SetBoxExtent);

    Lua.new_usertype<FLuaCircleCollider2DHandle>(
        "CircleCollider2D",
        sol::no_constructor,
        sol::base_classes, sol::bases<FLuaCollider2DHandle>(),
        "GetRadius", &FLuaCircleCollider2DHandle::GetRadius,
        "SetRadius", &FLuaCircleCollider2DHandle::SetRadius);
}
