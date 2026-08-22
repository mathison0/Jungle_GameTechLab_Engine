#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Game/Musou/MainBoss/AnimNotify_GolemFootstep.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

UCLASS()
class UAnimNotify_GolemFootstep : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_GolemFootstep() = default;
	~UAnimNotify_GolemFootstep() override = default;

	UPROPERTY(Edit, Save, Category="Golem Footstep", DisplayName="Volume")
	float Volume = 1.0f;

	UPROPERTY(Edit, Save, Category="Golem Footstep", DisplayName="Max Concurrent")
	int32 MaxConcurrent = 2;

	UPROPERTY(Edit, Save, Category="Golem Footstep", DisplayName="Min Interval Seconds")
	float MinIntervalSeconds = 0.04f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
