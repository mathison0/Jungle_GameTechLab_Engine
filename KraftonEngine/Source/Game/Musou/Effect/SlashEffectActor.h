#pragma once

#include "GameFramework/AActor.h"

#include "Source/Game/Musou/Effect/SlashEffectActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

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

protected:
	void LoadSlashAssets();
	void FinishSlash();

	void SetSlashAlpha(float Alpha);
	void SetSlashMaterialParams(float CoreAlpha, float GlowAlpha, float Dissolve, float NoiseScroll);

	void ResolveComponents();

protected:
	USceneComponent* SlashRootComponent = nullptr;
	UStaticMeshComponent* CoreMeshComponent = nullptr;
	UStaticMeshComponent* GlowMeshComponent = nullptr;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FString MeshPath = DefaultMeshPath;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FString CoreMaterialPath = "Content/Material/Editor/Slash_Core.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FString GlowMaterialPath = "Content/Material/Editor/Slash_Glow.mat";

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

	UStaticMeshComponent* RefractionMeshComponent = nullptr;
	UMaterialInterface* RefractionMaterial = nullptr;

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FString RefractionMaterialPath = "Content/Material/Editor/Slash_Refraction.mat";

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	FVector RefractionRelativeScale = FVector(1.35f, 1.35f, 1.35f);

	UPROPERTY(Edit, Save, Category = "SlashEffect")
	float RefractionStrength = 0.035f;

	float Age = 0.0f;
	bool bActive = false;
	FVector MoveDirection = FVector(1.0f, 0.0f, 0.0f);

	bool bDestroyOnFinish = false;
};