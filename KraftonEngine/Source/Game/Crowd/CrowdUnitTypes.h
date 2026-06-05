#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

enum class EUnitTeam : uint8
{
	Ally = 0,
	Enemy
};

enum class EUnitState : uint8
{
	Idle = 0,
	Chase,
	Attack,
	Dead
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
	float MaxHP = 100.0f;
	float MoveSpeed = 6.0f;
	float DetectRange = 18.0f;
	float AttackRange = 1.4f;
	float AttackDamage = 10.0f;
	float AttackCooldown = 1.0f;
	float Radius = 0.45f;
	float SeparationRadius = 1.1f;
	float SeparationWeight = 1.4f;
};

struct FCrowdUnit
{
	uint32 Generation = 1;
	bool bAlive = false;

	EUnitTeam Team = EUnitTeam::Enemy;
	EUnitState State = EUnitState::Idle;

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
	float AttackCooldownRemaining = 0.0f;
	float ThinkTimer = 0.0f;

	uint16 AnimState = 0;
	float AnimTime = 0.0f;
};

struct FDamageEvent
{
	FUnitHandle Target;
	float Damage = 0.0f;
	FVector HitDirection = FVector::ZeroVector;
	bool bCountAsPlayerKill = false;
};

struct FUnitRenderData
{
	FUnitHandle Handle;
	EUnitTeam Team = EUnitTeam::Enemy;
	EUnitState State = EUnitState::Idle;
	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	uint16 AnimState = 0;
	float AnimTime = 0.0f;
	bool bVisible = false;
};
