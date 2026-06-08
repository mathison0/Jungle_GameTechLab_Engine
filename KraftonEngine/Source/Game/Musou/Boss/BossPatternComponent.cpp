#include "Game/Musou/Boss/BossPatternComponent.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Core/Logging/Log.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Musou/Boss/BossPatternDataRegistry.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/AActor.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"

#include <algorithm>
#include <cmath>

namespace
{
	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	float YawFromDirectionXY(const FVector& Direction)
	{
		return std::atan2(Direction.Y, Direction.X) * (180.0f / 3.14159265f);
	}
}

void UBossPatternComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (Patterns.empty())
	{
		ConfigureFromBossId(BossId);
	}
	ResetRuntime();
}

void UBossPatternComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPatternEnabled)
	{
		return;
	}

	TickCooldowns(DeltaTime);

	UBattleComponent* Battle = ResolveBattleComponent();
	if (Battle && Battle->IsDead())
	{
		State = EBossPatternState::Dead;
		return;
	}

	APawn* Target = ResolvePlayerPawn();
	if (!Target || Patterns.empty())
	{
		PlayLocomotionMontageIfNeeded();
		return;
	}

	StateTime += (std::max)(DeltaTime, 0.0f);

	switch (State)
	{
	case EBossPatternState::Idle:
	case EBossPatternState::Approach:
	{
		const float Distance = DistanceToTargetXY(Target);
		const float HealthRatio = Battle ? Battle->GetHealthRatio() : 1.0f;
		if (const FBossPattern* Pattern = ChoosePattern(Distance, HealthRatio))
		{
			if (Pattern->IsSequence())
			{
				EnterSequence(*Pattern);
			}
			else
			{
				EnterTelegraph(*Pattern);
			}
			break;
		}

		State = EBossPatternState::Approach;
		PlayLocomotionMontageIfNeeded();
		MoveTowardTarget(Target, DeltaTime);
		break;
	}
	case EBossPatternState::Telegraph:
		if (CurrentPattern)
		{
			if (CurrentPattern->bFaceTargetBeforeAttack)
			{
				FaceTarget(Target);
			}
			if (CurrentPattern->bCanMoveDuringTelegraph)
			{
				MoveTowardTarget(Target, DeltaTime);
			}
			if (StateTime >= CurrentPattern->TelegraphTime)
			{
				EnterAttack();
			}
		}
		break;
	case EBossPatternState::Attack:
		if (CurrentPattern && !bAttackFired && StateTime >= CurrentPattern->AttackTime)
		{
			FireCurrentPattern();
		}
		if (CurrentPattern && StateTime >= CurrentPattern->AttackTime)
		{
			EnterRecovery();
		}
		break;
	case EBossPatternState::Sequence:
		TickSequence(DeltaTime);
		break;
	case EBossPatternState::Recovery:
		if (!CurrentPattern || StateTime >= CurrentPattern->RecoveryTime)
		{
			EnterIdle();
		}
		break;
	case EBossPatternState::Dead:
		break;
	}
}

bool UBossPatternComponent::ConfigureFromBossId(const FName& InBossId)
{
	BossId = InBossId;

	FBossPatternDataRegistry& Registry = FBossPatternDataRegistry::Get();
	Registry.EnsureFresh();

	if (const FBossDefinition* Definition = Registry.FindBoss(BossId))
	{
		return ConfigureFromDefinition(*Definition);
	}

	if (bDebugLog)
	{
		UE_LOG("[BossPattern] unknown BossId '%s'", BossId.ToString().c_str());
	}
	return false;
}

bool UBossPatternComponent::ConfigureFromDefinition(const FBossDefinition& Definition)
{
	BossId = Definition.BossId;
	Patterns = Definition.Patterns;
	IdleMontagePath = Definition.IdleMontagePath;
	IdleMontagePlayRate = Definition.IdleMontagePlayRate;
	IdleMontageBlendIn = Definition.IdleMontageBlendIn;
	RunMontagePath = Definition.RunMontagePath;
	RunMontagePlayRate = Definition.RunMontagePlayRate;
	RunMontageBlendIn = Definition.RunMontageBlendIn;

	if (UBattleComponent* Battle = ResolveBattleComponent())
	{
		Battle->MaxHealth = Definition.MaxHealth;
		Battle->AttackPower = Definition.AttackPower;
		Battle->bIsPlayerTeam = false;
		Battle->bAcceptKnockback = false;
	}

	if (AActor* Owner = GetOwner())
	{
		if (UCharacterMovementComponent* Movement = Owner->GetComponentByClass<UCharacterMovementComponent>())
		{
			Movement->MaxWalkSpeed = Definition.MoveSpeed;
		}
	}
	ApproachStopDistance = (std::max)(Definition.StopDistance, 0.0f);

	ResetRuntime();
	return !Patterns.empty();
}

void UBossPatternComponent::SetPatternEnabled(bool bEnabled)
{
	if (bPatternEnabled == bEnabled)
	{
		return;
	}

	bPatternEnabled = bEnabled;
	if (!bPatternEnabled && State != EBossPatternState::Dead)
	{
		EnterIdle();
	}
}

void UBossPatternComponent::PlayBossMontage(const FString& MontagePath, float PlayRate, float BlendIn)
{
	PlayMontagePath(MontagePath, PlayRate, BlendIn);
}

void UBossPatternComponent::ResetRuntime()
{
	PatternCooldowns.clear();
	CurrentPattern = nullptr;
	StepExecuted.clear();
	State = EBossPatternState::Idle;
	StateTime = 0.0f;
	bAttackFired = false;
}

APawn* UBossPatternComponent::ResolvePlayerPawn() const
{
	UWorld* World = GetWorld();
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	APlayerController* Controller = GameMode ? GameMode->GetPlayerController() : (World ? World->GetFirstPlayerController() : nullptr);
	return Controller ? Controller->GetPossessedPawn() : nullptr;
}

UBattleComponent* UBossPatternComponent::ResolveBattleComponent() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->GetComponentByClass<UBattleComponent>() : nullptr;
}

float UBossPatternComponent::DistanceToTargetXY(const APawn* Target) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return 1000000.0f;
	}
	return std::sqrt(DistanceSquaredXY(Owner->GetActorLocation(), Target->GetActorLocation()));
}

void UBossPatternComponent::FaceTarget(const APawn* Target)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	Owner->SetActorRotation(FRotator(0.0f, YawFromDirectionXY(ToTarget), 0.0f));
}

void UBossPatternComponent::MoveTowardTarget(APawn* Target, float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return;
	}

	FaceTarget(Target);
	if (DistanceToTargetXY(Target) <= ApproachStopDistance)
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		FVector Direction = Target->GetActorLocation() - Owner->GetActorLocation();
		Direction.Z = 0.0f;
		if (!Direction.IsNearlyZero())
		{
			Character->AddMovementInput(Direction.Normalized(), 1.0f);
		}
		return;
	}

	if (UCharacterMovementComponent* Movement = Owner->GetComponentByClass<UCharacterMovementComponent>())
	{
		FVector Direction = Target->GetActorLocation() - Owner->GetActorLocation();
		Direction.Z = 0.0f;
		if (!Direction.IsNearlyZero())
		{
			Movement->AddInputVector(Direction.Normalized(), 1.0f);
		}
	}
	(void)DeltaTime;
}

const FBossPattern* UBossPatternComponent::ChoosePattern(float Distance, float HealthRatio)
{
	int32 TotalWeight = 0;
	for (const FBossPattern& Pattern : Patterns)
	{
		if (Distance < Pattern.MinRange || Distance > Pattern.MaxRange)
		{
			continue;
		}
		if (HealthRatio < Pattern.HealthRatioMin || HealthRatio > Pattern.HealthRatioMax)
		{
			continue;
		}
		if (IsPatternOnCooldown(Pattern.Id))
		{
			continue;
		}
		TotalWeight += (std::max)(Pattern.Weight, 0);
	}

	if (TotalWeight <= 0)
	{
		return nullptr;
	}

	RandomState = RandomState * 1664525u + 1013904223u;
	int32 Pick = static_cast<int32>(RandomState % static_cast<uint32>(TotalWeight));
	for (const FBossPattern& Pattern : Patterns)
	{
		if (Distance < Pattern.MinRange || Distance > Pattern.MaxRange
			|| HealthRatio < Pattern.HealthRatioMin || HealthRatio > Pattern.HealthRatioMax
			|| IsPatternOnCooldown(Pattern.Id))
		{
			continue;
		}

		Pick -= (std::max)(Pattern.Weight, 0);
		if (Pick < 0)
		{
			return &Pattern;
		}
	}

	return nullptr;
}

bool UBossPatternComponent::IsPatternOnCooldown(const FName& PatternId) const
{
	for (const auto& Entry : PatternCooldowns)
	{
		if (Entry.first == PatternId && Entry.second > 0.0f)
		{
			return true;
		}
	}
	return false;
}

void UBossPatternComponent::SetPatternCooldown(const FName& PatternId, float Cooldown)
{
	for (auto& Entry : PatternCooldowns)
	{
		if (Entry.first == PatternId)
		{
			Entry.second = (std::max)(Cooldown, 0.0f);
			return;
		}
	}
	PatternCooldowns.push_back({ PatternId, (std::max)(Cooldown, 0.0f) });
}

void UBossPatternComponent::TickCooldowns(float DeltaTime)
{
	for (auto& Entry : PatternCooldowns)
	{
		Entry.second = (std::max)(Entry.second - DeltaTime, 0.0f);
	}
}

void UBossPatternComponent::EnterIdle()
{
	CurrentPattern = nullptr;
	State = EBossPatternState::Idle;
	StateTime = 0.0f;
	bAttackFired = false;
}

void UBossPatternComponent::EnterTelegraph(const FBossPattern& Pattern)
{
	CurrentPattern = &Pattern;
	State = EBossPatternState::Telegraph;
	StateTime = 0.0f;
	bAttackFired = false;

	if (APawn* Target = ResolvePlayerPawn())
	{
		FaceTarget(Target);
	}
	PlayCurrentPatternMontage();
}

void UBossPatternComponent::EnterSequence(const FBossPattern& Pattern)
{
	CurrentPattern = &Pattern;
	State = EBossPatternState::Sequence;
	StateTime = 0.0f;
	bAttackFired = false;
	StepExecuted.assign(Pattern.Steps.size(), 0);

	if (Pattern.Id == FName("dash_slash"))
	{
		UE_LOG("[BossPattern] dash_slash sequence started");
	}
}

void UBossPatternComponent::EnterAttack()
{
	State = EBossPatternState::Attack;
	StateTime = 0.0f;
	bAttackFired = false;
}

void UBossPatternComponent::EnterRecovery()
{
	if (CurrentPattern)
	{
		SetPatternCooldown(CurrentPattern->Id, CurrentPattern->Cooldown);
	}
	State = EBossPatternState::Recovery;
	StateTime = 0.0f;
}

void UBossPatternComponent::TickSequence(float DeltaTime)
{
	if (!CurrentPattern)
	{
		EnterIdle();
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(CurrentPattern->Steps.size()); ++Index)
	{
		const FBossPatternStep& Step = CurrentPattern->Steps[Index];
		if (StateTime < Step.Time)
		{
			continue;
		}

		const bool bDurationStep = Step.Type == EBossPatternStepType::Dash;
		if (bDurationStep)
		{
			if (StateTime <= Step.Time + Step.Duration)
			{
				ApplyDashStep(Step, DeltaTime);
			}
			continue;
		}

		if (Index < static_cast<int32>(StepExecuted.size()) && StepExecuted[Index] != 0)
		{
			continue;
		}

		ExecuteStep(Step);
		if (Index < static_cast<int32>(StepExecuted.size()))
		{
			StepExecuted[Index] = 1;
		}
	}

	if (StateTime >= GetCurrentSequenceEndTime())
	{
		EnterRecovery();
	}
}

void UBossPatternComponent::FireCurrentPattern()
{
	if (!CurrentPattern || CurrentPattern->bUseAnimNotify)
	{
		bAttackFired = true;
		return;
	}

	FireAttackSpec(CurrentPattern->AttackSpecId);
	bAttackFired = true;
}

void UBossPatternComponent::FireAttackSpec(FName AttackSpecId)
{
	const FAttackSpec* Spec = FindMusouAttackSpec(AttackSpecId);
	if (!Spec)
	{
		if (bDebugLog)
		{
			UE_LOG("[BossPattern] unknown attack spec '%s'", AttackSpecId.ToString().c_str());
		}
		return;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	if (!Owner || !GameMode)
	{
		return;
	}

	float AttackPower = 10.0f;
	if (UBattleComponent* Battle = ResolveBattleComponent())
	{
		AttackPower = Battle->GetAttackPower();
	}

	FMusouAttackEvent Event;
	Event.Attacker = Cast<APawn>(Owner);
	Event.Spec = *Spec;
	Event.Origin = Owner->GetActorLocation();
	Event.Forward = Owner->GetActorForward();
	Event.Damage = AttackPower * Spec->DamageMult;
	Event.bFromPlayer = false;

	GameMode->BroadcastAttack(Event);
}

void UBossPatternComponent::PlayCurrentPatternMontage()
{
	if (!CurrentPattern || CurrentPattern->MontagePath.empty())
	{
		return;
	}

	PlayMontagePath(CurrentPattern->MontagePath, CurrentPattern->PlayRate, CurrentPattern->BlendIn);
}

void UBossPatternComponent::PlayLocomotionMontageIfNeeded()
{
	if (State == EBossPatternState::Approach && !RunMontagePath.empty())
	{
		PlayMontageIfDifferent(RunMontagePath, RunMontagePlayRate, RunMontageBlendIn);
		return;
	}

	if (State == EBossPatternState::Idle && !IdleMontagePath.empty())
	{
		PlayMontageIfDifferent(IdleMontagePath, IdleMontagePlayRate, IdleMontageBlendIn);
	}
}

void UBossPatternComponent::PlayMontageIfDifferent(const FString& MontagePath, float PlayRate, float BlendIn)
{
	if (MontagePath.empty())
	{
		return;
	}

	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->GetComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}
	if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(AnimInstance))
	{
		if (!LuaAnim->ScriptFile.empty())
		{
			return;
		}
	}

	UAnimMontage* Montage = FAnimationManager::Get().LoadMontage(MontagePath);
	if (!Montage || AnimInstance->IsMontagePlaying(Montage))
	{
		return;
	}

	AnimInstance->PlayMontage(Montage, FName::None, PlayRate, BlendIn);
}

void UBossPatternComponent::PlayMontagePath(const FString& MontagePath, float PlayRate, float BlendIn)
{
	if (MontagePath.empty())
	{
		return;
	}
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->GetComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (UAnimMontage* Montage = FAnimationManager::Get().LoadMontage(MontagePath))
	{
		AnimInstance->PlayMontage(Montage, FName::None, PlayRate, BlendIn);
	}
}

void UBossPatternComponent::ExecuteStep(const FBossPatternStep& Step)
{
	switch (Step.Type)
	{
	case EBossPatternStepType::FaceTarget:
		if (APawn* Target = ResolvePlayerPawn())
		{
			FaceTarget(Target);
		}
		break;
	case EBossPatternStepType::PlayMontage:
		PlayMontagePath(Step.MontagePath, CurrentPattern ? CurrentPattern->PlayRate : 1.0f, CurrentPattern ? CurrentPattern->BlendIn : 0.1f);
		break;
	case EBossPatternStepType::Attack:
		FireAttackSpec(Step.AttackSpecId);
		break;
	case EBossPatternStepType::SetInvincible:
		if (UBattleComponent* Battle = ResolveBattleComponent())
		{
			Battle->SetInvincible(Step.bValue);
		}
		break;
	case EBossPatternStepType::WarningRim:
		if (AActor* Owner = GetOwner())
		{
			if (UHitFlashComponent* HitFlash = Owner->GetComponentByClass<UHitFlashComponent>())
			{
				HitFlash->PlayFlash(
					Step.Color,
					Step.Duration,
					Step.Intensity,
					Step.RimIntensity,
					Step.RimPower,
					Step.FillAmount);
			}
		}
		break;
	case EBossPatternStepType::SpawnProjectile:
	case EBossPatternStepType::SpawnEffect:
		if (bDebugLog)
		{
			UE_LOG("[BossPattern] step type is parsed but spawn backend is not wired yet");
		}
		break;
	case EBossPatternStepType::Wait:
	case EBossPatternStepType::Dash:
	default:
		break;
	}
}

void UBossPatternComponent::ApplyDashStep(const FBossPatternStep& Step, float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || Step.Speed <= 0.0f)
	{
		return;
	}

	FVector Direction = Owner->GetActorForward();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Owner->GetComponentByClass<UCharacterMovementComponent>())
	{
		Movement->AddRootMotionDelta(FTransform(
			FVector(Step.Speed * DeltaTime, 0.0f, 0.0f),
			FQuat::Identity,
			FVector::OneVector));
		return;
	}

	Owner->AddActorWorldOffset(Direction.Normalized() * Step.Speed * DeltaTime);
}

float UBossPatternComponent::GetCurrentSequenceEndTime() const
{
	if (!CurrentPattern)
	{
		return 0.0f;
	}

	float EndTime = CurrentPattern->RecoveryTime;
	for (const FBossPatternStep& Step : CurrentPattern->Steps)
	{
		EndTime = (std::max)(EndTime, Step.Time + Step.Duration);
	}
	return EndTime;
}
