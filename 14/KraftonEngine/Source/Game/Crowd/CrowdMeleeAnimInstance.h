#pragma once

#include "Game/Crowd/CrowdUnitAnimInstance.h"
#include "Object/FName.h"

#include "Source/Game/Crowd/CrowdMeleeAnimInstance.generated.h"

class UAnimSequenceBase;

UCLASS()
class UCrowdMeleeAnimInstance : public UCrowdUnitAnimInstance
{
public:
	GENERATED_BODY()

	UCrowdMeleeAnimInstance();

	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;

	void SetMeleeAnimationSet(const FCrowdMeleeAnimationSet& InAnimationSet);
	const FCrowdMeleeAnimationSet& GetMeleeAnimationSet() const { return AnimationSet; }

private:
	void BuildMeleeGraph();
	UAnimSequenceBase* LoadSequence(const FSoftObjectPtr& SequencePath) const;
	FName GetDesiredMeleeStateName() const;
	FName ComputeDesiredMeleeStateName() const;
	void UpdateStableMeleeState(float DeltaSeconds);
	void LogMeleeAnimStateIfChanged();
	bool WantsMeleeState(FName StateName) const;

private:
	FCrowdMeleeAnimationSet AnimationSet;
	FName StableMeleeStateName = FName::None;
	float StableMeleeStateElapsedTime = 0.0f;
	FName LastLoggedMeleeStateName = FName::None;
	EUnitState LastLoggedCrowdState = EUnitState::Idle;
	bool bHasLoggedMeleeState = false;
};
