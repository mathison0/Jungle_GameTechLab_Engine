#include "Game/Crowd/CrowdUnitAnimInstance.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Crowd/CrowdUnitVisualActor.h"

UCrowdUnitAnimInstance::UCrowdUnitAnimInstance()
{
	bAutoDriveSpeed = false;
}

void UCrowdUnitAnimInstance::NativeInitializeAnimation()
{
	bAutoDriveSpeed = false;
	Super::NativeInitializeAnimation();
	bAutoDriveSpeed = false;
}

void UCrowdUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	bAutoDriveSpeed = false;
	Super::NativeUpdateAnimation(DeltaSeconds);

	Speed = 0.0f;
	LastCrowdState = EUnitState::Idle;

	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	ACrowdUnitVisualActor* VisualActor = MeshComp ? Cast<ACrowdUnitVisualActor>(MeshComp->GetOwner()) : nullptr;
	if (!VisualActor || !VisualActor->IsVisualActive())
	{
		return;
	}

	LastCrowdState = VisualActor->GetCrowdState();
	if (LastCrowdState == EUnitState::Chase)
	{
		Speed = VisualActor->GetCrowdSpeed();
	}
}
