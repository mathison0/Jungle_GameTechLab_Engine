#include "Game/Crowd/CrowdUnitVisualActor.h"

#include "Animation/AnimationMode.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"

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

void ACrowdUnitVisualActor::InitializeVisual(ULargeScaleUnitManagerComponent* InManager, USkeletalMesh* InMesh, UClass* InAnimClass)
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
	Speed = InRenderData.Speed;
	bVisualActive = InRenderData.bVisible;

	SetActorLocation(InRenderData.Position);
	SetActorRotation(InRenderData.Rotation);
	SetVisible(bVisualActive);

	bNeedsTick = bVisualActive;
	MeshComponent->SetComponentTickEnabled(bVisualActive);
}

void ACrowdUnitVisualActor::DeactivateVisual()
{
	UnitHandle = {};
	UnitState = EUnitState::Dead;
	UnitCombatType = EUnitCombatType::Melee;
	Speed = 0.0f;
	bVisualActive = false;
	bNeedsTick = false;

	if (MeshComponent)
	{
		MeshComponent->SetComponentTickEnabled(false);
	}

	SetVisible(false);
}

void ACrowdUnitVisualActor::Tick(float DeltaTime)
{
	(void)DeltaTime;
}
