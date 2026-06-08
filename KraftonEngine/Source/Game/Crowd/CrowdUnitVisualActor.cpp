#include "Game/Crowd/CrowdUnitVisualActor.h"

#include "Animation/AnimationMode.h"
#include "Animation/AnimationTickLOD.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Crowd/CrowdMeleeAnimInstance.h"
#include "Game/Crowd/LargeScaleUnitManagerComponent.h"
#include "Materials/Material.h"
#include "Mesh/Skeletal/SkeletalMesh.h"

namespace
{
	EAnimationTickLOD ToAnimationTickLOD(ECrowdUnitLOD LOD)
	{
		switch (LOD)
		{
		case ECrowdUnitLOD::Simple:
			return EAnimationTickLOD::QuarterRate;
		case ECrowdUnitLOD::Formation:
			return EAnimationTickLOD::LowRate;
		case ECrowdUnitLOD::Dormant:
			return EAnimationTickLOD::Frozen;
		case ECrowdUnitLOD::Full:
		default:
			return EAnimationTickLOD::FullRate;
		}
	}
}

ACrowdUnitVisualActor::ACrowdUnitVisualActor()
{
	bNeedsTick = false;
}

USkeletalMeshComponent* ACrowdUnitVisualActor::EnsureMeshComponent()
{
	if (MeshComponent)
	{
		return MeshComponent;
	}

	MeshComponent = AddComponent<USkeletalMeshComponent>();
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetComponentTickEnabled(false);

	if (HasActorBegunPlay())
	{
		MeshComponent->BeginPlay();
		MeshComponent->SetComponentTickEnabled(false);
	}

	return MeshComponent;
}

void ACrowdUnitVisualActor::InitializeVisual(
	ULargeScaleUnitManagerComponent* InManager,
	USkeletalMesh* InMesh,
	const TArray<UMaterialInterface*>& InMaterials,
	UClass* InAnimClass,
	const FCrowdMeleeAnimationSet& InMeleeAnimationSet)
{
	Manager = InManager;
	USkeletalMeshComponent* VisualMeshComponent = EnsureMeshComponent();
	if (!VisualMeshComponent)
	{
		return;
	}

	const bool bMeshChanged = CurrentMesh != InMesh;
	if (bMeshChanged)
	{
		CurrentMesh = InMesh;
		VisualMeshComponent->SetSkeletalMesh(InMesh);
	}

	if (bMeshChanged || CurrentMaterials != InMaterials)
	{
		CurrentMaterials = InMaterials;
		const int32 MaterialCount = static_cast<int32>(VisualMeshComponent->GetOverrideMaterials().size());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			UMaterialInterface* Material = MaterialIndex < static_cast<int32>(InMaterials.size())
				? InMaterials[MaterialIndex]
				: nullptr;
			VisualMeshComponent->SetMaterial(MaterialIndex, Material);
		}
		BuildDynamicMaterials();
		ApplyHitFlashAmount(0.0f);
	}

	if (CurrentAnimClass != InAnimClass)
	{
		CurrentAnimClass = InAnimClass;
		VisualMeshComponent->SetAnimInstanceClass(InAnimClass);
	}

	VisualMeshComponent->SetAnimationMode(InAnimClass ? EAnimationMode::AnimationCustom : EAnimationMode::None);

	if (CurrentMeleeAnimationSet != InMeleeAnimationSet)
	{
		CurrentMeleeAnimationSet = InMeleeAnimationSet;
	}

	if (UCrowdMeleeAnimInstance* MeleeAnim = Cast<UCrowdMeleeAnimInstance>(VisualMeshComponent->GetAnimInstance()))
	{
		MeleeAnim->SetMeleeAnimationSet(CurrentMeleeAnimationSet);
	}
}

void ACrowdUnitVisualActor::ApplyRenderData(const FUnitRenderData& InRenderData)
{
	if (!MeshComponent)
	{
		return;
	}

	UnitHandle = InRenderData.Handle;
	UnitState = InRenderData.State;
	UnitCombatType = InRenderData.CombatType;
	UnitLOD = InRenderData.LOD;
	Velocity = InRenderData.Velocity;
	Speed = InRenderData.Speed;
	CircleAroundDirectionSign = InRenderData.CircleAroundDirectionSign;
	AnimTime = InRenderData.AnimTime;
	bKnockDownGettingUp = InRenderData.bKnockDownGettingUp;
	bVisualActive = InRenderData.bVisible;

	SetActorLocation(InRenderData.Position);
	SetActorRotation(InRenderData.Rotation);
	SetVisible(bVisualActive);
	ApplyHitFlashAmount(bVisualActive ? InRenderData.HitFlashAmount : 0.0f);

	bNeedsTick = bVisualActive;
	MeshComponent->SetEnableAnimationTickLOD(UnitLOD != ECrowdUnitLOD::Full);
	MeshComponent->SetAnimationTickLOD(ToAnimationTickLOD(UnitLOD));
	MeshComponent->SetComponentTickEnabled(bVisualActive && UnitLOD != ECrowdUnitLOD::Dormant);
}

void ACrowdUnitVisualActor::DeactivateVisual()
{
	UnitHandle = {};
	UnitState = EUnitState::Dead;
	UnitCombatType = EUnitCombatType::Melee;
	UnitLOD = ECrowdUnitLOD::Full;
	Velocity = FVector::ZeroVector;
	Speed = 0.0f;
	CircleAroundDirectionSign = 1.0f;
	AnimTime = 0.0f;
	bKnockDownGettingUp = false;
	bVisualActive = false;
	bNeedsTick = false;

	if (MeshComponent)
	{
		ApplyHitFlashAmount(0.0f);
		MeshComponent->SetEnableAnimationTickLOD(false);
		MeshComponent->SetAnimationTickLOD(EAnimationTickLOD::FullRate);
		MeshComponent->SetComponentTickEnabled(false);
		const int32 MaterialCount = static_cast<int32>(MeshComponent->GetOverrideMaterials().size());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			MeshComponent->SetMaterial(MaterialIndex, nullptr);
		}
	}

	CurrentMaterials.clear();
	DynamicMaterials.clear();
	SetVisible(false);
}

void ACrowdUnitVisualActor::BuildDynamicMaterials()
{
	DynamicMaterials.clear();

	if (!MeshComponent)
	{
		return;
	}

	const TArray<UMaterialInterface*>& Materials = MeshComponent->GetOverrideMaterials();
	for (int32 Index = 0; Index < static_cast<int32>(Materials.size()); ++Index)
	{
		UMaterialInterface* SourceMaterial = MeshComponent->GetMaterial(Index);
		if (!SourceMaterial)
		{
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial = nullptr;
		if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(SourceMaterial))
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialInstance);
		}
		else if (UMaterial* Material = Cast<UMaterial>(SourceMaterial))
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(Material);
		}

		if (!DynamicMaterial)
		{
			continue;
		}

		MeshComponent->SetMaterial(Index, DynamicMaterial);
		DynamicMaterials.push_back(DynamicMaterial);
	}
}

void ACrowdUnitVisualActor::ApplyHitFlashAmount(float Amount)
{
	for (UMaterialInstanceDynamic* Material : DynamicMaterials)
	{
		if (!Material)
		{
			continue;
		}

		Material->SetScalarParameter("HitFlashAmount", Amount);
		Material->SetScalarParameter("HitFlashFillAmount", 0.15f);
		Material->SetScalarParameter("HitFlashRimIntensity", 3.0f);
		Material->SetScalarParameter("HitFlashRimPower", 3.0f);
		Material->SetVector4Parameter("HitFlashColor", FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

bool ACrowdUnitVisualActor::ShouldLogCrowdAnimationState() const
{
	return Manager && Manager->IsUnitAnimationStateLogEnabled();
}

float ACrowdUnitVisualActor::GetCrowdLocomotionIdleSpeedThreshold() const
{
	return Manager ? Manager->GetCrowdLocomotionIdleSpeedThreshold() : 0.15f;
}

void ACrowdUnitVisualActor::Tick(float DeltaTime)
{
	(void)DeltaTime;
}
