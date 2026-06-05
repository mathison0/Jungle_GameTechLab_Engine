#include "Component/Primitive/HitFlashComponent.h"

#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <algorithm>

UHitFlashComponent::UHitFlashComponent()
{
	bTickEnable = true;
}

void UHitFlashComponent::InitializeFromSkinnedMesh(USkinnedMeshComponent* InMesh)
{
	TargetSkinnedMesh = InMesh;
	BuildDynamicMaterials();
	ApplyFlashAmount(0.0f);
}

void UHitFlashComponent::PlayFlash()
{
	if (DynamicMaterials.empty())
	{
		BuildDynamicMaterials();
	}

	Age = 0.0f;
	bPlaying = true;

	ApplyFlashAmount(Intensity);
}

void UHitFlashComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlaying)
	{
		return;
	}

	Age += DeltaTime;

	const float SafeDuration = std::max(Duration, 0.001f);
	const float T = FMath::Clamp(Age / SafeDuration, 0.0f, 1.0f);

	const float Amount = Intensity * (1.0f - T);
	ApplyFlashAmount(Amount);

	if (T >= 1.0f)
	{
		bPlaying = false;
		ApplyFlashAmount(0.0f);
	}
}

void UHitFlashComponent::ApplyFlashAmount(float Amount)
{
	for (UMaterialInstanceDynamic* Material : DynamicMaterials)
	{
		if (!Material)
		{
			continue;
		}

		Material->SetScalarParameter("HitFlashAmount", Amount);
		Material->SetScalarParameter("HitFlashBloomIntensity", BloomIntensity);
		Material->SetVector4Parameter("HitFlashColor", FlashColor);
	}
}

void UHitFlashComponent::BuildDynamicMaterials()
{
	DynamicMaterials.clear();

	if (!TargetSkinnedMesh)
	{
		return;
	}

	const TArray<UMaterialInterface*>& Materials = TargetSkinnedMesh->GetOverrideMaterials();

	for (int32 Index = 0; Index < static_cast<int32>(Materials.size()); ++Index)
	{
		UMaterialInterface* SourceMaterial = TargetSkinnedMesh->GetMaterial(Index);
		if (!SourceMaterial)
		{
			continue;
		}

		UMaterial* ParentMaterial = nullptr;

		if (UMaterial* Material = Cast<UMaterial>(SourceMaterial))
		{
			ParentMaterial = Material;
		}
		else if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(SourceMaterial))
		{
			ParentMaterial = MaterialInstance->GetParent();
		}

		if (!ParentMaterial)
		{
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ParentMaterial);
		if (!DynamicMaterial)
		{
			continue;
		}

		DynamicMaterial->SetScalarParameter("HitFlashAmount", 0.0f);
		DynamicMaterial->SetScalarParameter("HitFlashBloomIntensity", BloomIntensity);
		DynamicMaterial->SetVector4Parameter("HitFlashColor", FlashColor);

		TargetSkinnedMesh->SetMaterial(Index, DynamicMaterial);
		DynamicMaterials.push_back(DynamicMaterial);
	}
}
