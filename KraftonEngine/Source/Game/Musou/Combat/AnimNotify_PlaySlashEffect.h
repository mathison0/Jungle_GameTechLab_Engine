#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Math/Vector.h"
#include "Object/Ptr/SoftObjectPtr.h"

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

	UPROPERTY(Edit, Save, Category = "SlashEffect|Assets", AssetType = "StaticMesh")
	FSoftObjectPtr MeshPath = "Content/Data/Effect/Slash_StaticMesh.uasset";

	UPROPERTY(Edit, Save, Category = "SlashEffect|Assets", AssetType = "Material")
	FSoftObjectPtr CoreMaterialPath = "Content/Material/Editor/Slash_Core.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect|Assets", AssetType = "Material")
	FSoftObjectPtr GlowMaterialPath = "Content/Material/Editor/Slash_Glow.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect|Assets", AssetType = "Material")
	FSoftObjectPtr RefractionMaterialPath = "Content/Material/Editor/Slash_Refraction.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect|LayerScale")
	FVector CoreRelativeScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|LayerScale")
	FVector GlowRelativeScale = FVector(1.25f, 1.25f, 1.25f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|LayerScale")
	FVector RefractionRelativeScale = FVector(1.35f, 1.35f, 1.35f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|Timing")
	float Lifetime = 0.22f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Scale")
	FVector StartScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|Scale")
	FVector PeakScale = FVector(2.2f, 2.2f, 2.2f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|Scale")
	FVector EndScale = FVector(2.8f, 2.8f, 2.8f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|Motion")
	float MoveSpeed = 3.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Visual")
	float GlowAlphaMultiplier = 0.55f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Visual")
	float CoreFadeOutSpeed = 1.15f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Visual")
	float GlowFadeOutSpeed = 0.75f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Dissolve")
	float DissolveStartTime = 0.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Dissolve")
	float DissolveEndValue = 0.7f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Noise")
	float NoiseScrollSpeed = 1.6f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Refraction")
	float RefractionStrength = 0.035f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
