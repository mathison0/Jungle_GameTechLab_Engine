#include "LuaEngineBinding.h"

#include <algorithm>

#include "Component/PrimitiveComponent.h"
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
}

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
		Actor ? Actor->GetActorLocation() : FVector(0.f, 0.f, 0.f)
	);
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
		Actor ? Actor->GetActorRotation().ToVector() : FVector(0.f, 0.f, 0.f)
	);
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
		Actor ? Actor->GetActorScale() : FVector(1.0f, 1.0f, 1.0f)
	);
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
		Actor ? Actor->GetActorForward() : FVector(0.0f, 0.0f, 1.0f)
	);
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
		SceneComponent ? SceneComponent->GetWorldLocation() : FVector(0.0f, 0.0f, 0.0f)
	);
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
		SceneComponent ? SceneComponent->GetRelativeLocation() : FVector(0.0f, 0.0f, 0.0f)
	);
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
		SceneComponent ? SceneComponent->GetForwardVector() : FVector(0.0f, 0.0f, 1.0f)
	);
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

void RegisterLuaEngineBindings(sol::state& Lua)
{
	Lua.new_usertype<FLuaActorHandle>(
		"Actor",
		sol::no_constructor,

		"IsValid", &FLuaActorHandle::IsValid,
		"GetName", &FLuaActorHandle::GetName,
		"GetClassName", &FLuaActorHandle::GetActorClassName,

		"GetLocation", &FLuaActorHandle::GetLocation,
		"SetLocation", &FLuaActorHandle::SetLocation,
        "AddWorldOffset", &FLuaActorHandle::AddWorldOffset,

		"GetRotation", & FLuaActorHandle::GetRotation,
		"SetRotation", & FLuaActorHandle::SetRotation,

		"GetScale", & FLuaActorHandle::GetScale,
		"SetScale", & FLuaActorHandle::SetScale,

		"GetForward", & FLuaActorHandle::GetForward,

		"IsVisible", & FLuaActorHandle::IsVisible,
		"SetVisible", & FLuaActorHandle::SetVisible,

		"GetComponent", & FLuaActorHandle::GetComponent,
		"GetComponents", & FLuaActorHandle::GetComponents
		);

	Lua.new_usertype<FLuaComponentHandle>(
		"Component",
		sol::no_constructor,

		"IsValid", & FLuaComponentHandle::IsValid,
		"GetName", & FLuaComponentHandle::GetName,
		"GetClassName", & FLuaComponentHandle::GetComponentClassName,
		"IsA", & FLuaComponentHandle::IsA,
		"GetOwner", & FLuaComponentHandle::GetOwner,
		"IsActive", & FLuaComponentHandle::IsActive,
		"SetActive", & FLuaComponentHandle::SetActive,

		// SceneComponent API
		"GetWorldLocation", & FLuaComponentHandle::GetWorldLocation,
		"SetWorldLocation", & FLuaComponentHandle::SetWorldLocation,
		"GetRelativeLocation", & FLuaComponentHandle::GetRelativeLocation,
		"SetRelativeLocation", & FLuaComponentHandle::SetRelativeLocation,
		"GetForwardVector", & FLuaComponentHandle::GetForwardVector,

		// PrimitiveComponent API
		"IsVisible", & FLuaComponentHandle::IsVisible,
		"SetVisibility", & FLuaComponentHandle::SetVisibility,
		"ShouldGenerateOverlapEvents", & FLuaComponentHandle::ShouldGenerateOverlapEvents,
		"SetGenerateOverlapEvents", & FLuaComponentHandle::SetGenerateOverlapEvents,
		"IsOverlappingActor", & FLuaComponentHandle::IsOverlappingActor,
		"IsOverlappingComponent", & FLuaComponentHandle::IsOverlappingComponent
	);
}
