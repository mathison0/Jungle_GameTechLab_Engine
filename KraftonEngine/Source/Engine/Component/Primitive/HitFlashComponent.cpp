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

	ActiveDuration = Duration;
	ActiveIntensity = Intensity;
	ActiveFillAmount = FillAmount;
	ActiveRimIntensity = RimIntensity;
	ActiveRimPower = RimPower;
	ActiveFlashColor = FlashColor;

	Age = 0.0f;
	bPlaying = true;

	ApplyFlashAmount(ActiveIntensity);
}

void UHitFlashComponent::PlayFlash(
	const FVector4& InColor,
	float InDuration,
	float InIntensity,
	float InRimIntensity,
	float InRimPower,
	float InFillAmount)
{
	if (DynamicMaterials.empty())
	{
		BuildDynamicMaterials();
	}

	ActiveDuration = (std::max)(InDuration, 0.001f);
	ActiveIntensity = (std::max)(InIntensity, 0.0f);
	ActiveFillAmount = (std::max)(InFillAmount, 0.0f);
	ActiveRimIntensity = (std::max)(InRimIntensity, 0.0f);
	ActiveRimPower = (std::max)(InRimPower, 0.001f);
	ActiveFlashColor = InColor;

	Age = 0.0f;
	bPlaying = true;

	ApplyFlashAmount(ActiveIntensity);
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

	const float SafeDuration = std::max(ActiveDuration, 0.001f);
	const float T = FMath::Clamp(Age / SafeDuration, 0.0f, 1.0f);

	const float Amount = ActiveIntensity * (1.0f - T);
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
		Material->SetScalarParameter("HitFlashFillAmount", ActiveFillAmount);
		Material->SetScalarParameter("HitFlashRimIntensity", ActiveRimIntensity);
		Material->SetScalarParameter("HitFlashRimPower", ActiveRimPower);
		Material->SetVector4Parameter("HitFlashColor", ActiveFlashColor);
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
		DynamicMaterial->SetScalarParameter("HitFlashFillAmount", FillAmount);
		DynamicMaterial->SetScalarParameter("HitFlashRimIntensity", RimIntensity);
		DynamicMaterial->SetScalarParameter("HitFlashRimPower", RimPower);
		DynamicMaterial->SetVector4Parameter("HitFlashColor", FlashColor);

		TargetSkinnedMesh->SetMaterial(Index, DynamicMaterial);
		DynamicMaterials.push_back(DynamicMaterial);
	}
}
