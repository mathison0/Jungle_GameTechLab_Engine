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

	void SetMeleeAnimationSet(const FCrowdMeleeAnimationSet& InAnimationSet);
	const FCrowdMeleeAnimationSet& GetMeleeAnimationSet() const { return AnimationSet; }

private:
	void BuildMeleeGraph();
	UAnimSequenceBase* LoadSequence(const FSoftObjectPtr& SequencePath) const;
	FName GetDesiredMeleeStateName() const;
	bool WantsMeleeState(FName StateName) const;

private:
	FCrowdMeleeAnimationSet AnimationSet;
};
