#include "Game/Lua/GameLuaBindings.h"

#include "sol/sol.hpp"

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "Game/Crowd/LargeScaleUnitManagerComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Lua/LuaScriptManager.h"
#include "Lua/LuaDocRegistry.h"

#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/ComboComponent.h"
#include "GameFramework/AActor.h"

// ============================================================
// 게임-특화 Lua 바인딩 등록 위치.
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
	// ── UBattleComponent — 히트 판정(lua)이 데미지 적용(C++)을 호출하는 진입점 ──
	FLuaDocRegistry::Get().BindType<UBattleComponent>(Lua, "BattleComponent")
		.Method("ApplyDamage",
			"---@param damage number\n---@param instigator Actor?\n---@return number\nfunction BattleComponent:ApplyDamage(damage, instigator) end",
			[](UBattleComponent& Battle, float Damage, AActor* Instigator) { return Battle.ApplyDamage(Damage, Instigator); })
		.Method("Heal",
			"---@param amount number\nfunction BattleComponent:Heal(amount) end",
			[](UBattleComponent& Battle, float Amount) { Battle.Heal(Amount); })
		.Method("Kill",
			"---@param instigator Actor?\nfunction BattleComponent:Kill(instigator) end",
			[](UBattleComponent& Battle, AActor* Instigator) { Battle.Kill(Instigator); })
		.Method("IsDead",
			"---@return boolean\nfunction BattleComponent:IsDead() end",
			[](UBattleComponent& Battle) { return Battle.IsDead(); })
		.Method("GetHealth",
			"---@return number\nfunction BattleComponent:GetHealth() end",
			[](UBattleComponent& Battle) { return Battle.GetHealth(); })
		.Method("GetMaxHealth",
			"---@return number\nfunction BattleComponent:GetMaxHealth() end",
			[](UBattleComponent& Battle) { return Battle.GetMaxHealth(); })
		.Method("GetHealthRatio",
			"---@return number\nfunction BattleComponent:GetHealthRatio() end",
			[](UBattleComponent& Battle) { return Battle.GetHealthRatio(); })
		.Method("GetAttackPower",
			"---@return number\nfunction BattleComponent:GetAttackPower() end",
			[](UBattleComponent& Battle) { return Battle.GetAttackPower(); });

	// ── UComboComponent — 콤보 체인 상태 (입력/몽타주 재생은 lua, 상태는 C++) ──
	FLuaDocRegistry::Get().BindType<UComboComponent>(Lua, "ComboComponent")
		.Method("TryAttack",
			"---@return boolean # true = 새 콤보 시작(1단) — 호출측이 1단 몽타주 재생\nfunction ComboComponent:TryAttack() end",
			[](UComboComponent& Combo) { return Combo.TryAttack(); })
		.Method("ConsumeQueuedAdvance",
			"---@return boolean # true = 단계 전진 — 호출측이 GetComboStep() 단계 몽타주 재생\nfunction ComboComponent:ConsumeQueuedAdvance() end",
			[](UComboComponent& Combo) { return Combo.ConsumeQueuedAdvance(); })
		.Method("ResetCombo",
			"function ComboComponent:ResetCombo() end",
			[](UComboComponent& Combo) { Combo.ResetCombo(); })
		.Method("IsComboActive",
			"---@return boolean\nfunction ComboComponent:IsComboActive() end",
			[](UComboComponent& Combo) { return Combo.IsComboActive(); })
		.Method("GetComboStep",
			"---@return integer # 0 = 비활성, 1..MaxComboSteps\nfunction ComboComponent:GetComboStep() end",
			[](UComboComponent& Combo) { return Combo.GetComboStep(); })
		.Method("IsComboWindowOpen",
			"---@return boolean\nfunction ComboComponent:IsComboWindowOpen() end",
			[](UComboComponent& Combo) { return Combo.IsComboWindowOpen(); });

	// ── 기존 Actor usertype 확장 — Musou 컴포넌트 접근자 ──
	sol::table ActorTable = Lua["Actor"];
	ActorTable.set_function("GetBattleComponent",
		[](AActor& Actor) { return Actor.GetComponentByClass<UBattleComponent>(); });
	ActorTable.set_function("GetComboComponent",
		[](AActor& Actor) { return Actor.GetComponentByClass<UComboComponent>(); });
	FLuaDocRegistry::Get().Type("Actor")
		.Method("---@return BattleComponent?\nfunction Actor:GetBattleComponent() end")
		.Method("---@return ComboComponent?\nfunction Actor:GetComboComponent() end");

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
