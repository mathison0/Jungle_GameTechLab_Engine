#include "Game/Musou/MainBoss/MainBossPatternComponent.h"

#include "Animation/AnimationManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/AActor.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	constexpr float Pi = 3.14159265f;
	constexpr float Phase2HealthRatio = 0.5f;

	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	float YawFromDirectionXY(const FVector& Direction)
	{
		return std::atan2(Direction.Y, Direction.X) * (180.0f / Pi);
	}

	FMainBossPatternStep MakeStep(
		const char* SequencePath,
		float MinRange,
		float MaxRange,
		float ChaseGiveUpRange,
		float ChaseGiveUpTime,
		float PlayRate = 1.0f)
	{
		FMainBossPatternStep Step;
		Step.SequencePath = SequencePath;
		Step.MinRange = MinRange;
		Step.MaxRange = MaxRange;
		Step.ChaseGiveUpRange = ChaseGiveUpRange;
		Step.ChaseGiveUpTime = ChaseGiveUpTime;
		Step.PlayRate = PlayRate;
		return Step;
	}

	void NormalizeStep(FMainBossPatternStep& Step)
	{
		Step.MaxRange = (std::max)(Step.MaxRange, Step.MinRange);
		Step.ChaseGiveUpRange = Step.ChaseGiveUpRange > 0.0f
			? (std::max)(Step.ChaseGiveUpRange, Step.MaxRange)
			: Step.MaxRange + 8.0f;
		Step.ChaseGiveUpTime = (std::max)(Step.ChaseGiveUpTime, 0.0f);
		Step.PlayRate = (std::max)(Step.PlayRate, 0.01f);
		Step.AimTurnSpeedDegPerSec = (std::max)(Step.AimTurnSpeedDegPerSec, 0.0f);
	}

	FMainBossPatternStep MakeThrowStep()
	{
		FMainBossPatternStep Step = MakeStep(
			"Content/Data/GameJam/Golem_Boss/Golem_Boss_Throw_Attack_mixamo_com.uasset",
			6.0f,
			15.0f,
			22.0f,
			4.0f);
		Step.bAimUntilAttackNotify = true;
		Step.AimStopAttackId = FName("main_boss_throw_attack");
		Step.AimTurnSpeedDegPerSec = 360.0f;
		return Step;
	}
}

void UMainBossPatternComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	BuildDefaultPatterns();
	EnterDecide();
}

void UMainBossPatternComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPatternEnabled)
	{
		ResetThrowAim();
		PlayIdleIfNeeded();
		return;
	}

	TickCooldowns(DeltaTime);

	UBattleComponent* Battle = ResolveBattleComponent();
	if (Battle && Battle->IsDead())
	{
		ResetThrowAim();
		State = EMainBossPatternState::Dead;
		return;
	}

	UpdatePhaseTransition(Battle);

	APawn* Target = ResolvePlayerPawn();
	if (!Target || Patterns.empty())
	{
		ResetThrowAim();
		PlayIdleIfNeeded();
		return;
	}

	StateTime += (std::max)(DeltaTime, 0.0f);

	switch (State)
	{
	case EMainBossPatternState::Decide:
	{
		if (bPhase2Pending)
		{
			EnterBattlecry(Target);
			break;
		}

		const float Distance = DistanceToTargetXY(Target);
		if (const FMainBossPattern* Pattern = ChoosePattern(Distance))
		{
			const FMainBossPatternStep* FirstStep = GetFirstStep(*Pattern);
			if (!FirstStep)
			{
				EnterDecide();
				break;
			}

			if (IsStepInStartRange(*FirstStep, Distance))
			{
				EnterExecute(*Pattern, 0, Target);
			}
			else
			{
				EnterChase(*Pattern, 0);
				PlayChaseIfNeeded();
				MoveTowardTarget(Target, DeltaTime, FirstStep->MaxRange);
			}
			break;
		}

		PlayChaseIfNeeded();
		MoveTowardTarget(Target, DeltaTime, 5.0f);
		break;
	}
	case EMainBossPatternState::Chase:
	{
		const FMainBossPatternStep* Step = GetCurrentStep();
		if (!CurrentPattern || !Step)
		{
			EnterDecide();
			break;
		}

		const float Distance = DistanceToTargetXY(Target);
		if (ShouldGiveUpStepChase(*Step, Distance))
		{
			if (CurrentStepIndex > 0)
			{
				EnterRecovery();
			}
			else
			{
				EnterDecide();
			}
			break;
		}

		if (IsStepInStartRange(*Step, Distance))
		{
			EnterExecute(*CurrentPattern, CurrentStepIndex, Target);
			break;
		}

		PlayChaseIfNeeded();
		MoveTowardTarget(Target, DeltaTime, Step->MaxRange);
		break;
	}
	case EMainBossPatternState::Execute:
		TickThrowAim(DeltaTime, Target);
		if (!CurrentPattern || !GetCurrentStep() || StateTime >= ActiveExecutionTime)
		{
			AdvanceAfterStep(Target);
		}
		break;
	case EMainBossPatternState::Recovery:
		if (!CurrentPattern || StateTime >= CurrentPattern->RecoveryTime)
		{
			if (bPhase2Pending)
			{
				EnterBattlecry(Target);
			}
			else
			{
				EnterDecide();
			}
		}
		break;
	case EMainBossPatternState::Battlecry:
		if (StateTime >= ActiveExecutionTime)
		{
			bPhase2Entered = true;
			bPhase2Pending = false;
			EnterDecide();
		}
		break;
	case EMainBossPatternState::Dead:
		break;
	}
}

void UMainBossPatternComponent::SetPatternEnabled(bool bEnabled)
{
	if (bPatternEnabled == bEnabled)
	{
		return;
	}

	bPatternEnabled = bEnabled;
	if (!bPatternEnabled && State != EMainBossPatternState::Dead)
	{
		EnterDecide();
	}
}

void UMainBossPatternComponent::NotifyThrowAimStart()
{
	const FMainBossPatternStep* Step = GetCurrentStep();
	if (State != EMainBossPatternState::Execute || !Step || !Step->bAimUntilAttackNotify)
	{
		return;
	}

	bThrowAimActive = true;
}

void UMainBossPatternComponent::NotifyAttackBeforeBroadcast(FName AttackId)
{
	const FMainBossPatternStep* Step = GetCurrentStep();
	if (State != EMainBossPatternState::Execute
		|| !Step
		|| !Step->bAimUntilAttackNotify
		|| Step->AimStopAttackId != AttackId)
	{
		return;
	}

	FaceTarget(ResolvePlayerPawn());
	bThrowAimActive = false;
}

void UMainBossPatternComponent::BuildDefaultPatterns()
{
	if (!Patterns.empty())
	{
		return;
	}

	auto AddPattern = [this](FMainBossPattern Pattern)
	{
		if (Pattern.Steps.empty())
		{
			return;
		}

		Pattern.Phase = (std::max)(Pattern.Phase, 1);
		Pattern.Weight = (std::max)(Pattern.Weight, 0);
		Pattern.Cooldown = (std::max)(Pattern.Cooldown, 0.0f);
		Pattern.RecoveryTime = (std::max)(Pattern.RecoveryTime, 0.0f);
		for (FMainBossPatternStep& Step : Pattern.Steps)
		{
			NormalizeStep(Step);
		}
		Patterns.push_back(std::move(Pattern));
	};

	auto AddSingle = [&AddPattern](
		const char* Id,
		int32 Phase,
		const FMainBossPatternStep& Step,
		float Cooldown,
		float RecoveryTime,
		int32 Weight)
	{
		FMainBossPattern Pattern;
		Pattern.Id = FName(Id);
		Pattern.Phase = Phase;
		Pattern.Cooldown = Cooldown;
		Pattern.RecoveryTime = RecoveryTime;
		Pattern.Weight = Weight;
		Pattern.Steps.push_back(Step);
		AddPattern(std::move(Pattern));
	};

	AddSingle(
		"attack_slow",
		1,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Attack_Slow_mixamo_com.uasset", 0.0f, 5.2f, 13.2f, 3.0f),
		2.4f,
		1.0f,
		4);
	AddSingle(
		"attack_kick",
		1,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Attack_Kick_mixamo_com.uasset", 0.0f, 4.2f, 12.0f, 3.0f),
		2.5f,
		0.9f,
		4);
	AddSingle(
		"attack",
		1,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Attack_mixamo_com.uasset", 0.0f, 5.0f, 13.0f, 3.0f),
		2.0f,
		1.0f,
		5);
	AddSingle(
		"combo2_v1",
		1,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Combo2_Attack_Ver1_mixamo_com.uasset", 0.0f, 5.3f, 13.3f, 3.3f),
		3.5f,
		1.1f,
		3);
	AddSingle(
		"combo2_v2",
		1,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Combo2_Attack_Ver2_mixamo_com.uasset", 0.0f, 5.3f, 13.3f, 3.3f),
		3.5f,
		1.1f,
		3);
	AddSingle(
		"jump_attack",
		1,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Jump_Attack_mixamo_com.uasset", 3.0f, 9.5f, 18.0f, 4.0f),
		6.0f,
		1.3f,
		2);
	AddSingle(
		"throw_attack",
		1,
		MakeThrowStep(),
		6.5f,
		1.1f,
		2);
	AddSingle(
		"combo3",
		2,
		MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Combo3_Attack_mixamo_com.uasset", 0.0f, 5.5f, 13.5f, 3.5f),
		5.5f,
		1.5f,
		3);

	FMainBossPattern ThrowJump;
	ThrowJump.Id = FName("throw_jump_chain");
	ThrowJump.Phase = 2;
	ThrowJump.Cooldown = 8.0f;
	ThrowJump.RecoveryTime = 1.4f;
	ThrowJump.Weight = 2;
	ThrowJump.Steps.push_back(MakeThrowStep());
	ThrowJump.Steps.push_back(MakeStep("Content/Data/GameJam/Golem_Boss/Golem_Boss_Jump_Attack_mixamo_com.uasset", 3.0f, 9.5f, 18.0f, 4.0f));
	AddPattern(std::move(ThrowJump));
}

APawn* UMainBossPatternComponent::ResolvePlayerPawn() const
{
	UWorld* World = GetWorld();
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	APlayerController* Controller = GameMode ? GameMode->GetPlayerController() : (World ? World->GetFirstPlayerController() : nullptr);
	return Controller ? Controller->GetPossessedPawn() : nullptr;
}

UBattleComponent* UMainBossPatternComponent::ResolveBattleComponent() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->GetComponentByClass<UBattleComponent>() : nullptr;
}

float UMainBossPatternComponent::DistanceToTargetXY(const APawn* Target) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return 1000000.0f;
	}
	return std::sqrt(DistanceSquaredXY(Owner->GetActorLocation(), Target->GetActorLocation()));
}

void UMainBossPatternComponent::FaceTarget(const APawn* Target)
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

void UMainBossPatternComponent::FaceTargetLimited(const APawn* Target, float DeltaTime, float TurnSpeedDegPerSec)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return;
	}

	if (TurnSpeedDegPerSec <= 0.0f)
	{
		FaceTarget(Target);
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const float DesiredYaw = YawFromDirectionXY(ToTarget);
	const float CurrentYaw = Owner->GetActorRotation().Yaw;
	float DeltaYaw = std::fmod(DesiredYaw - CurrentYaw + 180.0f, 360.0f);
	if (DeltaYaw < 0.0f)
	{
		DeltaYaw += 360.0f;
	}
	DeltaYaw -= 180.0f;

	const float MaxStep = TurnSpeedDegPerSec * (std::max)(DeltaTime, 0.0f);
	const float AppliedDelta = std::clamp(DeltaYaw, -MaxStep, MaxStep);
	Owner->SetActorRotation(FRotator(0.0f, CurrentYaw + AppliedDelta, 0.0f));
}

void UMainBossPatternComponent::MoveTowardTarget(APawn* Target, float DeltaTime, float StopDistance)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return;
	}

	FaceTarget(Target);
	if (DistanceToTargetXY(Target) <= (std::max)(StopDistance, 0.0f))
	{
		return;
	}

	FVector Direction = Target->GetActorLocation() - Owner->GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Character->AddMovementInput(Direction.Normalized(), 1.0f);
	}
	else if (UCharacterMovementComponent* Movement = Owner->GetComponentByClass<UCharacterMovementComponent>())
	{
		Movement->AddInputVector(Direction.Normalized(), 1.0f);
	}
	(void)DeltaTime;
}

void UMainBossPatternComponent::ResetThrowAim()
{
	bThrowAimActive = false;
}

void UMainBossPatternComponent::TickThrowAim(float DeltaTime, APawn* Target)
{
	if (!bThrowAimActive)
	{
		return;
	}

	const FMainBossPatternStep* Step = GetCurrentStep();
	if (!Step || !Step->bAimUntilAttackNotify)
	{
		ResetThrowAim();
		return;
	}

	FaceTargetLimited(Target, DeltaTime, Step->AimTurnSpeedDegPerSec);
}

void UMainBossPatternComponent::PlayAttackStartWarningRim()
{
	if (!bUseAttackStartWarningRim)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UHitFlashComponent* HitFlash = Owner ? Owner->GetComponentByClass<UHitFlashComponent>() : nullptr;
	if (!HitFlash)
	{
		return;
	}

	HitFlash->PlayFlash(
		AttackStartWarningRimColor,
		AttackStartWarningRimDuration,
		AttackStartWarningRimIntensity,
		AttackStartWarningRimRimIntensity,
		AttackStartWarningRimRimPower,
		AttackStartWarningRimFillAmount);
}

int32 UMainBossPatternComponent::GetCurrentPhase() const
{
	return bPhase2Entered ? 2 : 1;
}

void UMainBossPatternComponent::UpdatePhaseTransition(const UBattleComponent* Battle)
{
	if (!Battle || bPhase2Entered || bPhase2Pending)
	{
		return;
	}

	if (Battle->GetHealthRatio() <= Phase2HealthRatio)
	{
		bPhase2Pending = true;
	}
}

const FMainBossPatternStep* UMainBossPatternComponent::GetCurrentStep() const
{
	if (!CurrentPattern || CurrentStepIndex < 0 || CurrentStepIndex >= static_cast<int32>(CurrentPattern->Steps.size()))
	{
		return nullptr;
	}
	return &CurrentPattern->Steps[CurrentStepIndex];
}

const FMainBossPatternStep* UMainBossPatternComponent::GetFirstStep(const FMainBossPattern& Pattern) const
{
	return Pattern.Steps.empty() ? nullptr : &Pattern.Steps[0];
}

const FMainBossPattern* UMainBossPatternComponent::ChoosePattern(float Distance)
{
	int32 TotalWeight = 0;
	for (const FMainBossPattern& Pattern : Patterns)
	{
		if (IsPatternSelectable(Pattern, Distance))
		{
			TotalWeight += (std::max)(Pattern.Weight, 0);
		}
	}

	if (TotalWeight <= 0)
	{
		return nullptr;
	}

	RandomState = RandomState * 1664525u + 1013904223u;

	int32 Pick = static_cast<int32>(RandomState % static_cast<uint32>(TotalWeight));
	for (const FMainBossPattern& Pattern : Patterns)
	{
		if (!IsPatternSelectable(Pattern, Distance))
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

bool UMainBossPatternComponent::IsPatternSelectable(const FMainBossPattern& Pattern, float Distance) const
{
	const FMainBossPatternStep* FirstStep = GetFirstStep(Pattern);
	if (!FirstStep || Pattern.Weight <= 0)
	{
		return false;
	}
	if (Pattern.Phase > GetCurrentPhase())
	{
		return false;
	}
	if (Distance < FirstStep->MinRange || Distance > FirstStep->ChaseGiveUpRange)
	{
		return false;
	}
	return !IsPatternOnCooldown(Pattern.Id);
}

bool UMainBossPatternComponent::IsStepInStartRange(const FMainBossPatternStep& Step, float Distance) const
{
	return Distance >= Step.MinRange && Distance <= Step.MaxRange;
}

bool UMainBossPatternComponent::ShouldGiveUpStepChase(const FMainBossPatternStep& Step, float Distance) const
{
	return Distance < Step.MinRange
		|| Distance > Step.ChaseGiveUpRange
		|| StateTime > Step.ChaseGiveUpTime;
}

bool UMainBossPatternComponent::IsPatternOnCooldown(const FName& PatternId) const
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

void UMainBossPatternComponent::SetPatternCooldown(const FName& PatternId, float Cooldown)
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

void UMainBossPatternComponent::TickCooldowns(float DeltaTime)
{
	for (auto& Entry : PatternCooldowns)
	{
		Entry.second = (std::max)(Entry.second - DeltaTime, 0.0f);
	}
}

void UMainBossPatternComponent::EnterDecide()
{
	ResetThrowAim();
	CurrentPattern = nullptr;
	CurrentStepIndex = 0;
	State = EMainBossPatternState::Decide;
	StateTime = 0.0f;
	ActiveExecutionTime = 0.0f;
}

void UMainBossPatternComponent::EnterChase(const FMainBossPattern& Pattern, int32 StepIndex)
{
	ResetThrowAim();
	CurrentPattern = &Pattern;
	CurrentStepIndex = StepIndex;
	State = EMainBossPatternState::Chase;
	StateTime = 0.0f;
	ActiveExecutionTime = 0.0f;
}

void UMainBossPatternComponent::EnterExecute(const FMainBossPattern& Pattern, int32 StepIndex, APawn* Target)
{
	CurrentPattern = &Pattern;
	CurrentStepIndex = StepIndex;
	State = EMainBossPatternState::Execute;
	StateTime = 0.0f;
	ResetThrowAim();

	const FMainBossPatternStep* Step = GetCurrentStep();
	if (!Step)
	{
		ActiveExecutionTime = 0.2f;
		return;
	}

	FaceTarget(Target);
	PlayAttackStartWarningRim();
	UAnimSequence* Sequence = PlaySequencePath(Step->SequencePath, false, Step->PlayRate, true);

	ActiveExecutionTime = 0.2f;
	if (Sequence)
	{
		ActiveExecutionTime = (std::max)(ActiveExecutionTime, Sequence->GetPlayLength() / (std::max)(Step->PlayRate, 0.01f));
	}
}

void UMainBossPatternComponent::EnterRecovery()
{
	ResetThrowAim();
	if (CurrentPattern)
	{
		SetPatternCooldown(CurrentPattern->Id, CurrentPattern->Cooldown);
	}
	State = EMainBossPatternState::Recovery;
	CurrentStepIndex = 0;
	StateTime = 0.0f;
	ActiveExecutionTime = 0.0f;
	PlayIdleIfNeeded();
}

void UMainBossPatternComponent::EnterBattlecry(APawn* Target)
{
	ResetThrowAim();
	CurrentPattern = nullptr;
	CurrentStepIndex = 0;
	State = EMainBossPatternState::Battlecry;
	StateTime = 0.0f;

	FaceTarget(Target);
	UAnimSequence* Sequence = PlaySequencePath(BattlecrySequencePath, false, 1.0f, true);
	ActiveExecutionTime = Sequence ? (std::max)(Sequence->GetPlayLength(), 0.2f) : 1.0f;
}

void UMainBossPatternComponent::AdvanceAfterStep(APawn* Target)
{
	if (!CurrentPattern)
	{
		EnterRecovery();
		return;
	}

	const int32 NextStepIndex = CurrentStepIndex + 1;
	if (NextStepIndex >= static_cast<int32>(CurrentPattern->Steps.size()))
	{
		EnterRecovery();
		return;
	}

	const FMainBossPatternStep& NextStep = CurrentPattern->Steps[NextStepIndex];
	const float Distance = DistanceToTargetXY(Target);
	if (Distance < NextStep.MinRange || Distance > NextStep.ChaseGiveUpRange)
	{
		EnterRecovery();
		return;
	}

	if (IsStepInStartRange(NextStep, Distance))
	{
		EnterExecute(*CurrentPattern, NextStepIndex, Target);
	}
	else
	{
		EnterChase(*CurrentPattern, NextStepIndex);
		PlayChaseIfNeeded();
	}
}

void UMainBossPatternComponent::PlayIdleIfNeeded()
{
	PlaySequencePath(IdleSequencePath, true, 1.0f, false);
}

void UMainBossPatternComponent::PlayChaseIfNeeded()
{
	PlaySequencePath(ChaseSequencePath.empty() ? IdleSequencePath : ChaseSequencePath, true, 1.0f, false);
}

UAnimSequence* UMainBossPatternComponent::PlaySequencePath(const FString& SequencePath, bool bLooping, float PlayRate, bool bForceRestart)
{
	if (SequencePath.empty())
	{
		return nullptr;
	}
	if (!bForceRestart && CurrentSequencePath == SequencePath)
	{
		return nullptr;
	}

	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->GetComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh)
	{
		return nullptr;
	}

	UAnimSequence* Sequence = FAnimationManager::Get().LoadAnimation(SequencePath);
	if (!Sequence)
	{
		if (bDebugLog)
		{
			UE_LOG("[MainBoss] animation load failed: %s", SequencePath.c_str());
		}
		return nullptr;
	}

	Mesh->PlayAnimation(Sequence, bLooping);
	Mesh->SetPlayRate((std::max)(PlayRate, 0.01f));
	CurrentSequencePath = SequencePath;
	return Sequence;
}
