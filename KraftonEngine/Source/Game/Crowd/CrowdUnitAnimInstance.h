#pragma once

#include "Animation/Instance/CharacterAnimInstance.h"
#include "Game/Crowd/CrowdUnitTypes.h"

#include "Source/Game/Crowd/CrowdUnitAnimInstance.generated.h"

UCLASS()
class UCrowdUnitAnimInstance : public UCharacterAnimInstance
{
public:
	GENERATED_BODY()

	UCrowdUnitAnimInstance();

	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;

	EUnitState GetLastCrowdState() const { return LastCrowdState; }

private:
	EUnitState LastCrowdState = EUnitState::Idle;
};
