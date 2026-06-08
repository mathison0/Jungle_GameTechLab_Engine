#pragma once

#include "Animation/AnimInstance.h"
#include "Game/Crowd/CrowdUnitTypes.h"

#include "Source/Game/Crowd/CrowdUnitAnimInstance.generated.h"

UCLASS()
class UCrowdUnitAnimInstance : public UAnimInstance
{
public:
	GENERATED_BODY()

	UCrowdUnitAnimInstance();

	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;

	EUnitState GetLastCrowdState() const { return LastCrowdState; }
	EUnitCombatType GetLastCrowdCombatType() const { return LastCrowdCombatType; }
	ECrowdUnitLOD GetLastCrowdLOD() const { return LastCrowdLOD; }
	float GetCrowdSpeed() const { return Speed; }
	float GetCrowdMoveForwardAmount() const { return MoveForwardAmount; }
	float GetCrowdMoveRightAmount() const { return MoveRightAmount; }
	float GetCrowdCircleAroundDirectionSign() const { return CircleAroundDirectionSign; }
	float GetCrowdAnimTime() const { return AnimTime; }
	float GetCrowdLocomotionIdleSpeedThreshold() const { return LocomotionIdleSpeedThreshold; }
	bool IsCrowdKnockDownGettingUp() const { return bKnockDownGettingUp; }

	UPROPERTY(Edit, Category="Animation|Crowd", DisplayName="Speed", Min=0.0f, Max=100.0f, Speed=0.5f)
	float Speed = 0.0f;

	UPROPERTY(Edit, Category="Animation|Crowd", DisplayName="Move Forward Amount", Min=-1.0f, Max=1.0f, Speed=0.05f)
	float MoveForwardAmount = 0.0f;

	UPROPERTY(Edit, Category="Animation|Crowd", DisplayName="Move Right Amount", Min=-1.0f, Max=1.0f, Speed=0.05f)
	float MoveRightAmount = 0.0f;

protected:
	EUnitState LastCrowdState = EUnitState::Idle;
	EUnitCombatType LastCrowdCombatType = EUnitCombatType::Melee;
	ECrowdUnitLOD LastCrowdLOD = ECrowdUnitLOD::Full;
	float CircleAroundDirectionSign = 1.0f;
	float AnimTime = 0.0f;
	float LocomotionIdleSpeedThreshold = 0.15f;
	bool bKnockDownGettingUp = false;
};
