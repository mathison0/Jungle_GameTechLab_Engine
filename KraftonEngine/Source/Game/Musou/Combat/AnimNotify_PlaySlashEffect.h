#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Math/Vector.h"

#include "Source/Game/Musou/Combat/AnimNotify_PlaySlashEffect.generated.h"

UCLASS()
class UAnimNotify_PlaySlashEffect : public UAnimNotify
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector LocationOffset = FVector(1.0f, 0.0f, 0.5f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector RotationOffset = FVector(0.0f, 90.0f, 0.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	bool bOnlyPlayer = true;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
