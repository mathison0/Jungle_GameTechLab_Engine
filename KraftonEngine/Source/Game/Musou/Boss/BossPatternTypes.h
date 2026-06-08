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
	SetInvincible,
	WarningRim
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
	FVector4 Color = FVector4(1.0f, 0.05f, 0.02f, 1.0f);
	float Speed = 0.0f;
	float Intensity = 2.5f;
	float RimIntensity = 5.0f;
	float RimPower = 3.0f;
	float FillAmount = 0.0f;
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

enum class EBossSequenceStepType : uint8
{
	Wait = 0,
	Dialogue,
	PlayMontage,
	BlendCamera,
	RestoreCamera,
	PlayAudio,
	LockPlayer,
	UnlockPlayer,
	SetBossPatternEnabled,
	FaceBossToPlayer,
	StopMovement,
	SetInvincible,
	DestroyActor,
	CameraFadeIn,
	CameraFadeOut,
	CameraShake,
	WarningRim
};

struct FBossSequenceStep
{
	EBossSequenceStepType Type = EBossSequenceStepType::Wait;
	float Time = 0.0f;
	float Duration = 0.0f;

	FString Text;
	FString MontagePath;
	FString SoundPath;
	float PlayRate = 1.0f;
	float BlendIn = 0.1f;
	float Volume = 1.0f;
	FVector CameraOffset = FVector(-4.5f, -3.0f, 2.0f);
	float LookAtHeight = 1.5f;
	float FOV = 0.87266463f;
	bool bValue = false;
	bool bLoop = false;
	FString CameraTag;
	FString ShakeAssetPath;
	float ShakeScale = 1.0f;
	FVector4 Color = FVector4(1.0f, 0.05f, 0.02f, 1.0f);
	float Intensity = 2.5f;
	float RimIntensity = 5.0f;
	float RimPower = 3.0f;
	float FillAmount = 0.0f;
};

struct FBossPhaseSequence
{
	FName Id = FName::None;
	float HealthRatio = 0.5f;
	bool bOnce = true;
	TArray<FBossSequenceStep> Steps;
};

struct FBossDefinition
{
	FName BossId = FName::None;
	FString DisplayName;
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
	TArray<FBossSequenceStep> IntroSteps;
	TArray<FBossSequenceStep> DeathSteps;
	TArray<FBossPhaseSequence> PhaseSequences;
	TArray<FBossPattern> Patterns;

	bool IsValid() const { return BossId.IsValid() && !Patterns.empty(); }
};
