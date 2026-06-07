#include "Game/Crowd/CrowdUnitVisualActor.h"

#include "Animation/AnimationMode.h"
#include "Animation/AnimationTickLOD.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Crowd/CrowdMeleeAnimInstance.h"
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
	UClass* InAnimClass,
	const FCrowdMeleeAnimationSet& InMeleeAnimationSet)
{
	Manager = InManager;
	USkeletalMeshComponent* VisualMeshComponent = EnsureMeshComponent();
	if (!VisualMeshComponent)
	{
		return;
	}

	if (CurrentMesh != InMesh)
	{
		CurrentMesh = InMesh;
		VisualMeshComponent->SetSkeletalMesh(InMesh);
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
	bVisualActive = InRenderData.bVisible;

	SetActorLocation(InRenderData.Position);
	SetActorRotation(InRenderData.Rotation);
	SetVisible(bVisualActive);

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
	bVisualActive = false;
	bNeedsTick = false;

	if (MeshComponent)
	{
		MeshComponent->SetEnableAnimationTickLOD(false);
		MeshComponent->SetAnimationTickLOD(EAnimationTickLOD::FullRate);
		MeshComponent->SetComponentTickEnabled(false);
	}

	SetVisible(false);
}

void ACrowdUnitVisualActor::Tick(float DeltaTime)
{
	(void)DeltaTime;
}
