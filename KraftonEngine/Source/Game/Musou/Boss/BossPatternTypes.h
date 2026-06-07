#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"

enum class EBossPatternState : uint8
{
	Idle = 0,
	Approach,
	Telegraph,
	Attack,
	Sequence,
	Recovery,
	Dead
};

enum class EBossPatternStepType : uint8
{
	Wait = 0,
	FaceTarget,
	PlayMontage,
	Attack,
	Dash,
	SpawnProjectile,
	SpawnEffect,
	SetInvincible
};

struct FBossPatternStep
{
	EBossPatternStepType Type = EBossPatternStepType::Wait;
	float Time = 0.0f;
	float Duration = 0.0f;

	FName AttackSpecId = FName::None;
	FString MontagePath;
	FString ProjectileClassName;
	FString EffectPath;
	FVector Offset = FVector::ZeroVector;
	float Speed = 0.0f;
	bool bValue = false;
};

struct FBossPattern
{
	FName Id = FName::None;
	FName AttackSpecId = FName::None;
	FString MontagePath;
	FString SequencePath;

	float MinRange = 0.0f;
	float MaxRange = 5.0f;
	float Cooldown = 2.0f;
	float TelegraphTime = 0.4f;
	float AttackTime = 0.15f;
	float RecoveryTime = 0.8f;
	float PlayRate = 1.0f;
	float BlendIn = 0.1f;

	float HealthRatioMin = 0.0f;
	float HealthRatioMax = 1.0f;
	int32 Weight = 1;

	bool bFaceTargetBeforeAttack = true;
	bool bCanMoveDuringTelegraph = false;
	bool bUseAnimNotify = false;
	TArray<FBossPatternStep> Steps;

	bool IsSequence() const { return !Steps.empty(); }
};

struct FBossDefinition
{
	FName BossId = FName::None;
	float MaxHealth = 1000.0f;
	float AttackPower = 20.0f;
	float MoveSpeed = 3.0f;
	float StopDistance = 2.5f;
	FString MeshPath;
	FString AnimScript;
	FString IdleMontagePath;
	float IdleMontagePlayRate = 1.0f;
	float IdleMontageBlendIn = 0.1f;
	FString RunMontagePath;
	float RunMontagePlayRate = 1.0f;
	float RunMontageBlendIn = 0.1f;
	TArray<FBossPattern> Patterns;

	bool IsValid() const { return BossId.IsValid() && !Patterns.empty(); }
};
