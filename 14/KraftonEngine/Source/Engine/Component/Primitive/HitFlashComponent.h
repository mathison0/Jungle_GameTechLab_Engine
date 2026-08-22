#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Component/Primitive/HitFlashComponent.generated.h"

class USkinnedMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class UHitFlashComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	UHitFlashComponent();

	void InitializeFromSkinnedMesh(USkinnedMeshComponent* InMesh);
	void PlayFlash();
	void PlayFlash(
		const FVector4& InColor,
		float InDuration,
		float InIntensity,
		float InRimIntensity,
		float InRimPower = 3.0f,
		float InFillAmount = 0.0f);

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void ApplyFlashAmount(float Amount);
	void BuildDynamicMaterials();

	USkinnedMeshComponent* TargetSkinnedMesh = nullptr;

	TArray<UMaterialInstanceDynamic*> DynamicMaterials;

	UPROPERTY(Edit, Save, Category = "HitFlash")
	float Duration = 0.12f;

	UPROPERTY(Edit, Save, Category = "HitFlash")
	float Intensity = 1.0f;

	UPROPERTY(Edit, Save, Category = "HitFlash")
	float FillAmount = 0.15f;

	UPROPERTY(Edit, Save, Category = "HitFlash")
	float RimIntensity = 3.0f;

	UPROPERTY(Edit, Save, Category = "HitFlash")
	float RimPower = 3.0f;

	UPROPERTY(Edit, Save, Category = "HitFlash", Type = Color4)
	FVector4 FlashColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

	float ActiveDuration = 0.12f;
	float ActiveIntensity = 1.0f;
	float ActiveFillAmount = 0.15f;
	float ActiveRimIntensity = 3.0f;
	float ActiveRimPower = 3.0f;
	FVector4 ActiveFlashColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

	float Age = 0.0f;
	bool bPlaying = false;
};
