#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Game/Musou/Effect/SlashEffectActor.h"
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

	UPROPERTY(Edit, Category = "SlashEffect")
	bool bPreviewInEditor = true;

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

	UPROPERTY(Edit, Save, Category = "SlashEffect|Mask")
	float RevealDurationRatio = 0.18f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Mask")
	float RevealSoftness = 0.08f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Mask")
	float EdgeSoftness = 0.12f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Mask")
	float TailFadeStart = 1.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Mask")
	float TrailLength = 0.55f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|Refraction")
	float RefractionStrength = 0.035f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float CoreScreenThickness = 2.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float CoreScreenThicknessStrength = 0.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float GlowScreenThickness = 8.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float GlowScreenThicknessStrength = 1.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float RefractionScreenThickness = 3.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float RefractionScreenThicknessStrength = 0.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|DissolveEdge", Type = Color4)
	FVector4 DissolveEdgeColor = FVector4(1.0f, 0.75f, 0.35f, 1.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect|DissolveEdge")
	float DissolveEdgeWidth = 0.06f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|DissolveEdge")
	float CoreDissolveEdgeIntensity = 1.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|DissolveEdge")
	float GlowDissolveEdgeIntensity = 3.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	bool bSpawnArcSparks = false;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark", AssetType = "UParticleSystem")
	FSoftObjectPtr ArcSparkParticlePath = "Content/Particle/Slash_ArcSpark.uasset";

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	int32 ArcSparkCount = 18;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkRadius = 1.2f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkAngleMin = -70.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkAngleMax = 70.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark", Enum = ESlashArcSparkPlaneMode)
	ESlashArcSparkPlaneMode ArcSparkPlaneMode = ESlashArcSparkPlaneMode::YZ;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkOutwardSpeed = 1.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkTangentSpeed = 0.4f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkUpSpeed = 0.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkPositionJitter = 0.03f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ArcSpark")
	float ArcSparkSpeedJitter = 0.1f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
