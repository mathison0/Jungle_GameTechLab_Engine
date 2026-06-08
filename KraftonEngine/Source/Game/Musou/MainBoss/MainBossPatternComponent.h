#pragma once

#include "Component/ActorComponent.h"
#include "Game/Musou/MainBoss/MainBossPatternTypes.h"
#include "Math/Vector.h"

#include <utility>

#include "Source/Game/Musou/MainBoss/MainBossPatternComponent.generated.h"

class APawn;
class UBattleComponent;
class UAnimSequence;

UCLASS()
class UMainBossPatternComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UMainBossPatternComponent() = default;
	~UMainBossPatternComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void SetPatternEnabled(bool bEnabled);
	bool IsPatternEnabled() const { return bPatternEnabled; }
	EMainBossPatternState GetState() const { return State; }
	void NotifyThrowAimStart();
	void NotifyAttackBeforeBroadcast(FName AttackId);

	UPROPERTY(Edit, Save, Category="Main Boss|Animation", DisplayName="Idle Sequence", AssetType="UAnimSequence")
	FString IdleSequencePath = "Content/Data/GameJam/Golem_Boss/Golem_Boss_Idle_mixamo_com.uasset";

	UPROPERTY(Edit, Save, Category="Main Boss|Animation", DisplayName="Chase Sequence", AssetType="UAnimSequence")
	FString ChaseSequencePath = "Content/Data/GameJam/Golem_Boss/Golem_Boss_Walk_mixamo_com.uasset";

	UPROPERTY(Edit, Save, Category="Main Boss|Animation", DisplayName="Battlecry Sequence", AssetType="UAnimSequence")
	FString BattlecrySequencePath = "Content/Data/GameJam/Golem_Boss/Golem_Boss_Battlecry_mixamo_com.uasset";

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Use Attack Start Warning Rim")
	bool bUseAttackStartWarningRim = true;

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Attack Start Warning Rim Duration")
	float AttackStartWarningRimDuration = 0.35f;

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Attack Start Warning Rim Color", Type=Color4)
	FVector4 AttackStartWarningRimColor = FVector4(1.0f, 0.05f, 0.02f, 1.0f);

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Attack Start Warning Rim Intensity")
	float AttackStartWarningRimIntensity = 2.5f;

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Attack Start Warning Rim Rim Intensity")
	float AttackStartWarningRimRimIntensity = 5.0f;

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Attack Start Warning Rim Rim Power")
	float AttackStartWarningRimRimPower = 3.0f;

	UPROPERTY(Edit, Save, Category="Main Boss|Warning Rim", DisplayName="Attack Start Warning Rim Fill Amount")
	float AttackStartWarningRimFillAmount = 0.0f;

	UPROPERTY(Edit, Save, Category="Main Boss|Debug", DisplayName="Debug Log")
	bool bDebugLog = false;

private:
	void BuildDefaultPatterns();
	APawn* ResolvePlayerPawn() const;
	UBattleComponent* ResolveBattleComponent() const;
	float DistanceToTargetXY(const APawn* Target) const;
	void FaceTarget(const APawn* Target);
	void FaceTargetLimited(const APawn* Target, float DeltaTime, float TurnSpeedDegPerSec);
	void MoveTowardTarget(APawn* Target, float DeltaTime, float StopDistance);
	void ResetThrowAim();
	void TickThrowAim(float DeltaTime, APawn* Target);
	void PlayAttackStartWarningRim();

	int32 GetCurrentPhase() const;
	void UpdatePhaseTransition(const UBattleComponent* Battle);
	const FMainBossPatternStep* GetCurrentStep() const;
	const FMainBossPatternStep* GetFirstStep(const FMainBossPattern& Pattern) const;
	const FMainBossPattern* ChoosePattern(float Distance);
	bool IsPatternSelectable(const FMainBossPattern& Pattern, float Distance) const;
	bool IsStepInStartRange(const FMainBossPatternStep& Step, float Distance) const;
	bool ShouldGiveUpStepChase(const FMainBossPatternStep& Step, float Distance) const;
	bool IsPatternOnCooldown(const FName& PatternId) const;
	void SetPatternCooldown(const FName& PatternId, float Cooldown);
	void TickCooldowns(float DeltaTime);

	void EnterDecide();
	void EnterChase(const FMainBossPattern& Pattern, int32 StepIndex);
	void EnterExecute(const FMainBossPattern& Pattern, int32 StepIndex, APawn* Target);
	void EnterRecovery();
	void EnterBattlecry(APawn* Target);
	void AdvanceAfterStep(APawn* Target);

	void PlayIdleIfNeeded();
	void PlayChaseIfNeeded();
	UAnimSequence* PlaySequencePath(const FString& SequencePath, bool bLooping, float PlayRate, bool bForceRestart);

	TArray<FMainBossPattern> Patterns;
	TArray<std::pair<FName, float>> PatternCooldowns;
	const FMainBossPattern* CurrentPattern = nullptr;

	EMainBossPatternState State = EMainBossPatternState::Decide;
	int32 CurrentStepIndex = 0;
	float StateTime = 0.0f;
	float ActiveExecutionTime = 0.0f;
	bool bPhase2Pending = false;
	bool bPhase2Entered = false;
	bool bThrowAimActive = false;
	bool bPatternEnabled = true;
	uint32 RandomState = 0x2468ace1u;
	FString CurrentSequencePath;
};
