#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/Ptr/SoftObjectPtr.h"

enum class EUnitTeam : uint8
{
	Ally = 0,
	Enemy
};

enum class EUnitCombatType : uint8
{
	Melee = 0,
	Ranged
};

enum class EUnitState : uint8
{
	Idle = 0,
	Move,
	Chase,
	CircleAround,
	Attack,
	Hit,
	KnockDown,
	Dead
};

enum class ECrowdUnitLOD : uint8
{
	Full = 0,
	Simple,
	Formation,
	Dormant
};

enum class ECrowdTargetKind : uint8
{
	None = 0,
	Unit,
	Player
};

struct FCrowdMeleeAnimationSet
{
	FSoftObjectPtr IdleSequencePath = "None";
	FSoftObjectPtr WalkForwardSequencePath = "None";
	FSoftObjectPtr WalkBackwardSequencePath = "None";
	FSoftObjectPtr RunForwardSequencePath = "None";
	FSoftObjectPtr RunBackwardSequencePath = "None";
	FSoftObjectPtr StrafeWalkLeftSequencePath = "None";
	FSoftObjectPtr StrafeWalkRightSequencePath = "None";
	FSoftObjectPtr StrafeRunLeftSequencePath = "None";
	FSoftObjectPtr StrafeRunRightSequencePath = "None";
	FSoftObjectPtr AttackSequencePath = "None";
	FSoftObjectPtr HitSequencePath = "None";
	FSoftObjectPtr KnockDownSequencePath = "None";
	FSoftObjectPtr GettingUpSequencePath = "None";
	FSoftObjectPtr DeadSequencePath = "None";

	bool operator==(const FCrowdMeleeAnimationSet& Other) const
	{
		return IdleSequencePath.ToString() == Other.IdleSequencePath.ToString()
			&& WalkForwardSequencePath.ToString() == Other.WalkForwardSequencePath.ToString()
			&& WalkBackwardSequencePath.ToString() == Other.WalkBackwardSequencePath.ToString()
			&& RunForwardSequencePath.ToString() == Other.RunForwardSequencePath.ToString()
			&& RunBackwardSequencePath.ToString() == Other.RunBackwardSequencePath.ToString()
			&& StrafeWalkLeftSequencePath.ToString() == Other.StrafeWalkLeftSequencePath.ToString()
			&& StrafeWalkRightSequencePath.ToString() == Other.StrafeWalkRightSequencePath.ToString()
			&& StrafeRunLeftSequencePath.ToString() == Other.StrafeRunLeftSequencePath.ToString()
			&& StrafeRunRightSequencePath.ToString() == Other.StrafeRunRightSequencePath.ToString()
			&& AttackSequencePath.ToString() == Other.AttackSequencePath.ToString()
			&& HitSequencePath.ToString() == Other.HitSequencePath.ToString()
			&& KnockDownSequencePath.ToString() == Other.KnockDownSequencePath.ToString()
			&& GettingUpSequencePath.ToString() == Other.GettingUpSequencePath.ToString()
			&& DeadSequencePath.ToString() == Other.DeadSequencePath.ToString();
	}

	bool operator!=(const FCrowdMeleeAnimationSet& Other) const
	{
		return !(*this == Other);
	}
};

struct FUnitHandle
{
	uint32 Index = UINT32_MAX;
	uint32 Generation = 0;

	bool IsValid() const
	{
		return Index != UINT32_MAX && Generation != 0;
	}

	bool operator==(const FUnitHandle& Other) const
	{
		return Index == Other.Index && Generation == Other.Generation;
	}
};

struct FUnitArchetype
{
	EUnitCombatType CombatType = EUnitCombatType::Melee;
	float MaxHP = 100.0f;
	float MoveSpeed = 6.0f;
	float DetectRange = 18.0f;
	float AttackRange = 1.4f;
	float AttackDamage = 10.0f;
	float AttackCooldown = 1.0f;
	float Radius = 0.45f;
	float SeparationRadius = 1.1f;
	float SeparationWeight = 1.4f;
	float LoseTargetRange = 24.0f;
};

struct FCrowdUnit
{
	uint32 Generation = 1;
	bool bAlive = false;

	EUnitTeam Team = EUnitTeam::Enemy;
	EUnitState State = EUnitState::Idle;
	FUnitArchetype Archetype;

	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float SpawnZ = 0.0f;
	FVector GroundNormal = FVector::UpVector;
	int32 GroundMissFrames = 0;
	bool bHasGround = false;

	float HP = 100.0f;
	float Radius = 0.45f;

	FUnitHandle Target;
	ECrowdTargetKind TargetKind = ECrowdTargetKind::None;
	FVector MoveGoal = FVector::ZeroVector;
	FVector LookAtLocation = FVector::ZeroVector;
	int32 CombatSlotIndex = -1;
	bool bHasCombatSlot = false;
	bool bHasAttackToken = false;
	float CircleAroundDirectionSign = 1.0f;
	float AttackCooldownRemaining = 0.0f;
	float ThinkTimer = 0.0f;
	float FriendlyBlockedTime = 0.0f;
	float StateTimeRemaining = 0.0f;
	float KnockbackTimeRemaining = 0.0f;
	FVector KnockbackVelocity = FVector::ZeroVector;
	float HitFlashTimeRemaining = 0.0f;
	float HitFlashDuration = 0.12f;
	float HitFlashIntensity = 1.0f;
	// 띄우기 (플레이어 launcher 공격) — 공중에 있는 동안 KnockDown 상태가 유지되고
	// MovementManager 가 Z 포물선을 적분한다. 착지 시 해제 후 잔여 다운 시간 소화.
	bool bAirborne = false;
	float AirborneVelZ = 0.0f;
	float KnockDownAnimDuration = 0.0f;
	float GettingUpAnimDuration = 0.0f;
	float KnockDownFlyingBackTimeRemaining = 0.0f;
	float KnockDownGettingUpTimeRemaining = 0.0f;
	bool bKnockDownGettingUp = false;
	ECrowdUnitLOD LOD = ECrowdUnitLOD::Full;
	float LODUpdateTimeRemaining = 0.0f;
	float LODAccumulatedDeltaTime = 0.0f;
	float SimulationDeltaTime = 0.0f;
	bool bSimulateThisFrame = true;

	uint16 AnimState = 0;
	float AnimTime = 0.0f;
};

inline bool IsCrowdUnitAliveForGameplay(const FCrowdUnit& Unit)
{
	return Unit.bAlive
		&& Unit.State != EUnitState::Dead;
}

inline bool IsCrowdUnitCombatActive(const FCrowdUnit& Unit)
{
	return IsCrowdUnitAliveForGameplay(Unit)
		&& Unit.LOD != ECrowdUnitLOD::Dormant;
}

inline bool ShouldSimulateCrowdUnitThisFrame(const FCrowdUnit& Unit)
{
	return IsCrowdUnitCombatActive(Unit) && Unit.bSimulateThisFrame;
}

inline bool IsCrowdUnitVisibleForRender(const FCrowdUnit& Unit)
{
	return Unit.bAlive && Unit.LOD != ECrowdUnitLOD::Dormant;
}

inline bool IsCrowdUnitControlLocked(EUnitState State)
{
	return State == EUnitState::Hit
		|| State == EUnitState::KnockDown
		|| State == EUnitState::Dead;
}

inline bool IsCrowdUnitMovingState(EUnitState State)
{
	return State == EUnitState::Move
		|| State == EUnitState::Chase
		|| State == EUnitState::CircleAround;
}

struct FDamageEvent
{
	FUnitHandle Target;
	float Damage = 0.0f;
	FVector HitDirection = FVector::ZeroVector;
	bool bCountAsPlayerKill = false;
	bool bTriggerHitFlash = false;
	bool bCanKnockDown = false;
	float KnockbackDistance = 0.0f;
	float KnockbackDuration = 0.0f;
	float LaunchVelocityZ = 0.0f;   // 띄우기 초기 수직 속도 (FAttackSpec.LaunchZ). 0 = 없음
};

struct FUnitRenderData
{
	FUnitHandle Handle;
	EUnitTeam Team = EUnitTeam::Enemy;
	EUnitCombatType CombatType = EUnitCombatType::Melee;
	EUnitState State = EUnitState::Idle;
	ECrowdUnitLOD LOD = ECrowdUnitLOD::Full;
	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Velocity = FVector::ZeroVector;
	uint16 AnimState = 0;
	float AnimTime = 0.0f;
	float Speed = 0.0f;
	float CircleAroundDirectionSign = 1.0f;
	float HitFlashAmount = 0.0f;
	bool bKnockDownGettingUp = false;
	bool bVisible = false;
};
