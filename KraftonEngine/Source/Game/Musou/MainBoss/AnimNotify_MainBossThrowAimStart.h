#pragma once

#include "Animation/Notify/AnimNotify.h"

#include "Source/Game/Musou/MainBoss/AnimNotify_MainBossThrowAimStart.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

UCLASS()
class UAnimNotify_MainBossThrowAimStart : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_MainBossThrowAimStart() = default;
	~UAnimNotify_MainBossThrowAimStart() override = default;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
