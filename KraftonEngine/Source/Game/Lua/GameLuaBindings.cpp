#include "Game/Lua/GameLuaBindings.h"

#include "sol/sol.hpp"

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "Game/Crowd/LargeScaleUnitManagerComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Lua/LuaScriptManager.h"

// ============================================================
// 게임-특화 Lua 바인딩 등록 위치 — 현재는 비어 있음.
//
// Engine 의 FLuaScriptManager 가 등록하는 일반 binding (AActor / APawn / FVector /
// UWorld / Anim 등) 만으로 동작하지 않는 game-specific usertype (ACarPawn /
// AGameStateXxx / 전용 enum 등) 이 도입되면 여기에 new_usertype 으로 추가한다.
//
// 호출 시점: UEngine::Init() 이 FLuaScriptManager::Initialize() 를 끝낸 직후.
// 등록은 EngineInitHooks 에 자동으로 걸려 GameEngine / EditorEngine 두 엔트리 모두
// 같은 바인딩이 적용된다 (PIE 호환).
// ============================================================
void RegisterGameLuaBindings(sol::state& Lua)
{
	Lua.new_enum("EUnitTeam", {
		std::pair<sol::string_view, EUnitTeam>{ "Player", EUnitTeam::Player },
		std::pair<sol::string_view, EUnitTeam>{ "Ally", EUnitTeam::Ally },
		std::pair<sol::string_view, EUnitTeam>{ "Enemy", EUnitTeam::Enemy },
	});

	Lua.new_usertype<FUnitHandle>("UnitHandle",
		sol::constructors<FUnitHandle()>(),
		"Index", &FUnitHandle::Index,
		"Generation", &FUnitHandle::Generation,
		"IsValid", &FUnitHandle::IsValid);

	Lua.new_usertype<ULargeScaleUnitManagerComponent>("LargeScaleUnitManagerComponent",
		"SpawnUnit", [](ULargeScaleUnitManagerComponent& Manager, EUnitTeam Team, const FVector& Position)
		{
			return Manager.SpawnUnit(Team, Position);
		},
		"SpawnUnits", [](ULargeScaleUnitManagerComponent& Manager, EUnitTeam Team, const FVector& Center, int32 Count, float Radius)
		{
			Manager.SpawnUnits(Team, Center, Count, Radius);
		},
		"DespawnUnit", &ULargeScaleUnitManagerComponent::DespawnUnit,
		"ClearUnits", &ULargeScaleUnitManagerComponent::ClearUnits,
		"ApplyRadialDamage", &ULargeScaleUnitManagerComponent::ApplyRadialDamage,
		"GetAliveCount", &ULargeScaleUnitManagerComponent::GetAliveCount,
		"GetTeamAliveCount", &ULargeScaleUnitManagerComponent::GetTeamAliveCount,
		"SetDebugDrawEnabled", &ULargeScaleUnitManagerComponent::SetDebugDrawEnabled,
		"IsDebugDrawEnabled", &ULargeScaleUnitManagerComponent::IsDebugDrawEnabled,
		"IsUnitAlive", &ULargeScaleUnitManagerComponent::IsUnitAlive,
		"GetUnitPosition", &ULargeScaleUnitManagerComponent::GetUnitPosition,
		"GetRenderDataCount", [](ULargeScaleUnitManagerComponent& Manager)
		{
			return static_cast<int32>(Manager.GetRenderData().size());
		});

	sol::table Crowd = Lua.create_named_table("Crowd");
	Crowd.set_function("GetOrCreateManager", [](sol::optional<AActor*> OwnerActor) -> ULargeScaleUnitManagerComponent*
	{
		if (!GEngine)
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorld();
		if (!World)
		{
			return nullptr;
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			if (ULargeScaleUnitManagerComponent* Manager = Actor->GetComponentByClass<ULargeScaleUnitManagerComponent>())
			{
				return Manager;
			}
		}

		AActor* Owner = OwnerActor.value_or(nullptr);
		return Owner ? Owner->AddComponent<ULargeScaleUnitManagerComponent>() : nullptr;
	});
}

// 자기-등록 — Editor / Game 측이 RegisterGameLuaBindings 함수명을 모르고도
// FEngineInitHooks::RunAll() 한 번이면 호출되도록 static initializer 로 등록.
namespace
{
	void RunRegisterGameLuaBindings()
	{
		RegisterGameLuaBindings(FLuaScriptManager::GetState());
	}

	struct GameLuaBindingsAutoReg
	{
		GameLuaBindingsAutoReg() { FEngineInitHooks::Register(&RunRegisterGameLuaBindings); }
	};

	static GameLuaBindingsAutoReg gAutoReg;
}
