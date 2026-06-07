#pragma once

#include "GameFramework/AActor.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Game/Musou/Effect/SlashEffectActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UParticleSystem;
class UParticleSystemComponent;

UENUM()
enum class ESlashArcSparkPlaneMode : uint8
{
	YZ,
	XZ,
	XY,
};

UCLASS()
class ASlashEffectActor : public AActor
{
public:
	GENERATED_BODY()

	ASlashEffectActor() = default;
	~ASlashEffectActor() override = default;

	static constexpr const char* DefaultMeshPath =
		"Content/Data/Effect/Slash_StaticMesh.uasset";

	static constexpr const char* DefaultMaterialPath =
		"Content/Material/Editor/Slash_Additive.mat";

	void InitDefaultComponents();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void PostDuplicate() override;
	void PostLoad() override;

	void ActivateSlash(const FVector& Location, const FVector& Rotation, const FVector& Direction);

	void SetDestroyOnFinish(bool bInDestroyOnFinish) { bDestroyOnFinish = bInDestroyOnFinish; }
	void ConfigureSlashEffect(
		const FSoftObjectPtr& InMeshPath,
		const FSoftObjectPtr& InCoreMaterialPath,
		const FSoftObjectPtr& InGlowMaterialPath,
		const FSoftObjectPtr& InRefractionMaterialPath,
		const FVector& InCoreRelativeScale,
		const FVector& InGlowRelativeScale,
		const FVector& InRefractionRelativeScale,
		float InGlowAlphaMultiplier,
		float InLifetime,
		const FVector& InStartScale,
		const FVector& InPeakScale,
		const FVector& InEndScale,
		float InMoveSpeed,
		const FVector& InRotationOffset,
		float InCoreFadeOutSpeed,
		float InGlowFadeOutSpeed,
		float InDissolveStartTime,
		float InDissolveEndValue,
		float InNoiseScrollSpeed,
		float InRefractionStrength,
		float InRevealDurationRatio,
		float InRevealSoftness,
		float InEdgeSoftness,
		float InTailFadeStart,
		float InTrailLength,
		float InCoreScreenThickness,
		float InCoreScreenThicknessStrength,
		float InGlowScreenThickness,
		float InGlowScreenThicknessStrength,
		float InRefractionScreenThickness,
		float InRefractionScreenThicknessStrength,
		const FVector4& InDissolveEdgeColor,
		float InDissolveEdgeWidth,
		float InCoreDissolveEdgeIntensity,
		float InGlowDissolveEdgeIntensity,
		bool bInSpawnArcSparks,
		const FSoftObjectPtr& InArcSparkParticlePath,
		int32 InArcSparkCount,
		float InArcSparkRadius,
		float InArcSparkAngleMin,
		float InArcSparkAngleMax,
		ESlashArcSparkPlaneMode InArcSparkPlaneMode,
		float InArcSparkOutwardSpeed,
		float InArcSparkTangentSpeed,
		float InArcSparkUpSpeed,
		float InArcSparkPositionJitter,
		float InArcSparkSpeedJitter);

protected:
	void LoadSlashAssets();
	void FinishSlash();

	void SetSlashAlpha(float Alpha);
	void SetSlashMaterialParams(float CoreAlpha, float GlowAlpha, float Dissolve, float NoiseScroll, float Reveal);
	void SpawnArcSparks();

	void ResolveComponents();

protected:
	USceneComponent* SlashRootComponent = nullptr;
	UStaticMeshComponent* CoreMeshComponent = nullptr;
	UStaticMeshComponent* GlowMeshComponent = nullptr;
	UParticleSystemComponent* ArcSparkComponent = nullptr;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	UParticleSystem* ArcSparkParticle = nullptr;

	UPROPERTY(Edit, Save, Category = "SlashEffect", AssetType = "StaticMesh")
	FSoftObjectPtr MeshPath = DefaultMeshPath;

	UPROPERTY(Edit, Save, Category = "SlashEffect", AssetType = "Material")
	FSoftObjectPtr CoreMaterialPath = "Content/Material/Editor/Slash_Core.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect", AssetType = "Material")
	FSoftObjectPtr GlowMaterialPath = "Content/Material/Editor/Slash_Glow.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector CoreRelativeScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector GlowRelativeScale = FVector(1.25f, 1.25f, 1.25f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float GlowAlphaMultiplier = 0.55f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float Lifetime = 0.22f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector StartScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector PeakScale = FVector(2.2f, 2.2f, 2.2f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector EndScale = FVector(2.8f, 2.8f, 2.8f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float MoveSpeed = 3.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector RotationOffset = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float CoreFadeOutSpeed = 1.15f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float GlowFadeOutSpeed = 0.75f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float DissolveStartTime = 0.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float DissolveEndValue = 0.7f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float NoiseScrollSpeed = 1.6f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float RevealDurationRatio = 0.18f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float RevealSoftness = 0.08f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float EdgeSoftness = 0.12f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float TailFadeStart = 1.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float TrailLength = 0.55f;

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

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float CoreScreenThickness = 2.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float CoreScreenThicknessStrength = 0.5f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float GlowScreenThickness = 8.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float GlowScreenThicknessStrength = 1.0f;

	UStaticMeshComponent* RefractionMeshComponent = nullptr;
	UMaterialInterface* RefractionMaterial = nullptr;

	UPROPERTY(Edit, Save, Category = "SlashEffect", AssetType = "Material")
	FSoftObjectPtr RefractionMaterialPath = "Content/Material/Editor/Slash_Refraction.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector RefractionRelativeScale = FVector(1.35f, 1.35f, 1.35f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float RefractionStrength = 0.035f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float RefractionScreenThickness = 3.0f;

	UPROPERTY(Edit, Save, Category = "SlashEffect|ScreenThickness")
	float RefractionScreenThicknessStrength = 0.5f;

	float Age = 0.0f;
	bool bActive = false;
	FVector MoveDirection = FVector(1.0f, 0.0f, 0.0f);

	bool bDestroyOnFinish = false;
};
