#pragma once

#include "Core/CoreTypes.h"
#include "sol.hpp"

class AActor;
class UActorComponent;
class UCollider2DComponent;

struct FLuaComponentHandle;
struct FLuaCollider2DHandle;

struct FLuaActorHandle
{
	uint32 UUID = 0;

	FLuaActorHandle() = default;
	explicit FLuaActorHandle(const AActor* InActor);

	AActor* Resolve() const;
	bool IsValid() const;

	FString GetName() const;
	FString GetActorClassName() const;

	sol::table GetLocation(sol::this_state State) const;
	bool SetLocation(const sol::object& Value) const;
	bool AddWorldOffset(const sol::object& Value) const;

	sol::table GetRotation(sol::this_state State) const;
	bool SetRotation(const sol::object& Value) const;

	sol::table GetScale(sol::this_state State) const;
	bool SetScale(const sol::object& Value) const;

	sol::table GetForward(sol::this_state State) const;

	bool IsVisible() const;
	bool SetVisible(bool bVisible) const;

	FLuaComponentHandle GetComponent(const sol::variadic_args& Args) const;
	sol::table GetComponents(sol::this_state State, const sol::variadic_args& Args) const;
};

struct FLuaComponentHandle
{
	uint32 UUID = 0;

	FLuaComponentHandle() = default;
	explicit FLuaComponentHandle(const UActorComponent* InComponent);

	UActorComponent* Resolve() const;
	bool IsValid() const;

	FString GetName() const;
	FString GetComponentClassName() const;
	bool IsA(const FString& ClassName) const;

	FLuaActorHandle GetOwner() const;

	bool IsActive() const;
	bool SetActive(bool bActive) const;

	// SceneComponent API
	sol::table GetWorldLocation(sol::this_state State) const;
	bool SetWorldLocation(const sol::object& Value) const;

	sol::table GetRelativeLocation(sol::this_state State) const;
	bool SetRelativeLocation(const sol::object& Value) const;

	sol::table GetForwardVector(sol::this_state State) const;

	bool LookAt(const sol::object& State) const;

	sol::object LuaCast(sol::this_state State, const FString& ClassName) const;

	// PrimitiveComponent API
	bool IsVisible() const;
	bool SetVisibility(bool bVisible) const;

	bool ShouldGenerateOverlapEvents() const;
	bool SetGenerateOverlapEvents(bool bGenerate) const;

	bool IsOverlappingActor(const FLuaActorHandle& OtherActor) const;
	bool IsOverlappingComponent(const FLuaComponentHandle& OtherComponent) const;
};

struct FLuaCollider2DHandle : public FLuaComponentHandle
{
	using FLuaComponentHandle::FLuaComponentHandle;

	UCollider2DComponent* ResolveCollider() const;

	// Common Collider2D methods
	sol::table GetShapeWorldLocation2D(sol::this_state State) const;
	float GetCollisionPlaneZ() const;
};

struct FLuaBoxCollider2DHandle : public FLuaCollider2DHandle
{
	using FLuaCollider2DHandle::FLuaCollider2DHandle;

	sol::table GetBoxExtent(sol::this_state State) const;
	bool SetBoxExtent(const sol::object& Value) const;
};

struct FLuaCircleCollider2DHandle : public FLuaCollider2DHandle
{
	using FLuaCollider2DHandle::FLuaCollider2DHandle;

	float GetRadius() const;
	bool SetRadius(float Radius) const;
};

void RegisterLuaEngineBindings(sol::state& Lua);
