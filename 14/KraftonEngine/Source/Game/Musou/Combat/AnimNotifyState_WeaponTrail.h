#pragma once

#include "Animation/Notify/AnimNotifyState.h"

#include "Source/Game/Musou/Combat/AnimNotifyState_WeaponTrail.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class UWeaponTrailComponent;

UCLASS()
class UAnimNotifyState_WeaponTrail : public UAnimNotifyState
{
public:
	GENERATED_BODY()

	UAnimNotifyState_WeaponTrail() = default;
	~UAnimNotifyState_WeaponTrail() override = default;

	UPROPERTY(Edit, Save, Category = "WeaponTrail", DisplayName = "Clear Trail On Begin")
	bool bClearTrailOnBegin = true;

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

private:
	UWeaponTrailComponent* ResolveWeaponTrailComponent(USkeletalMeshComponent* MeshComp) const;

	UPROPERTY(Edit, Save, Category = "WeaponTrail", DisplayName = "Trail Component Name")
	FName TrailComponentName = FName::None;
};
