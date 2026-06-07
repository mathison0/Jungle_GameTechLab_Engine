#pragma once

#include "Component/ActorComponent.h"
#include "Game/Musou/Boss/BossPatternTypes.h"

#include "Source/Game/Musou/Boss/BossPatternComponent.generated.h"

class APawn;
class UBattleComponent;

UCLASS()
class UBossPatternComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UBossPatternComponent() = default;
	~UBossPatternComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	bool ConfigureFromBossId(const FName& InBossId);
	bool ConfigureFromDefinition(const FBossDefinition& Definition);

	const FName& GetBossId() const { return BossId; }
	EBossPatternState GetState() const { return State; }

	UPROPERTY(Edit, Save, Category="Boss", DisplayName="Boss Id")
	FName BossId = FName("knight_boss");

	UPROPERTY(Edit, Save, Category="Boss|Debug", DisplayName="Debug Log")
	bool bDebugLog = false;

private:
	void ResetRuntime();
	APawn* ResolvePlayerPawn() const;
	UBattleComponent* ResolveBattleComponent() const;
	float DistanceToTargetXY(const APawn* Target) const;
	void FaceTarget(const APawn* Target);
	void MoveTowardTarget(APawn* Target, float DeltaTime);

	const FBossPattern* ChoosePattern(float Distance, float HealthRatio);
	bool IsPatternOnCooldown(const FName& PatternId) const;
	void SetPatternCooldown(const FName& PatternId, float Cooldown);
	void TickCooldowns(float DeltaTime);

	void EnterIdle();
	void EnterTelegraph(const FBossPattern& Pattern);
	void EnterAttack();
	void EnterSequence(const FBossPattern& Pattern);
	void EnterRecovery();
	void TickSequence(float DeltaTime);
	void FireCurrentPattern();
	void FireAttackSpec(FName AttackSpecId);
	void PlayCurrentPatternMontage();
	void PlayLocomotionMontageIfNeeded();
	void PlayMontageIfDifferent(const FString& MontagePath, float PlayRate, float BlendIn);
	void PlayMontagePath(const FString& MontagePath, float PlayRate, float BlendIn);
	void ExecuteStep(const FBossPatternStep& Step);
	void ApplyDashStep(const FBossPatternStep& Step, float DeltaTime);
	float GetCurrentSequenceEndTime() const;

	TArray<FBossPattern> Patterns;
	TArray<std::pair<FName, float>> PatternCooldowns;
	const FBossPattern* CurrentPattern = nullptr;
	TArray<uint8> StepExecuted;

	EBossPatternState State = EBossPatternState::Idle;
	float StateTime = 0.0f;
	bool bAttackFired = false;
	uint32 RandomState = 0x13572468u;
	float ApproachStopDistance = 2.5f;
	FString IdleMontagePath;
	float IdleMontagePlayRate = 1.0f;
	float IdleMontageBlendIn = 0.1f;
	FString RunMontagePath;
	float RunMontagePlayRate = 1.0f;
	float RunMontageBlendIn = 0.1f;
};
