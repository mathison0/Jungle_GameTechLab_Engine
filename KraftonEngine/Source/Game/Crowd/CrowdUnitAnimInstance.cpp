#include "Game/Crowd/CrowdUnitAnimInstance.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Crowd/CrowdUnitVisualActor.h"

#include <cmath>

UCrowdUnitAnimInstance::UCrowdUnitAnimInstance()
{
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UCrowdUnitAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UCrowdUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	Speed = 0.0f;
	MoveForwardAmount = 0.0f;
	MoveRightAmount = 0.0f;
	LastCrowdState = EUnitState::Idle;
	LastCrowdCombatType = EUnitCombatType::Melee;
	LastCrowdLOD = ECrowdUnitLOD::Full;
	CircleAroundDirectionSign = 1.0f;

	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	ACrowdUnitVisualActor* VisualActor = MeshComp ? Cast<ACrowdUnitVisualActor>(MeshComp->GetOwner()) : nullptr;
	if (!VisualActor || !VisualActor->IsVisualActive())
	{
		return;
	}

	LastCrowdState = VisualActor->GetCrowdState();
	LastCrowdCombatType = VisualActor->GetCrowdCombatType();
	LastCrowdLOD = VisualActor->GetCrowdLOD();
	CircleAroundDirectionSign = VisualActor->GetCrowdCircleAroundDirectionSign();
	if (IsCrowdUnitMovingState(LastCrowdState))
	{
		Speed = VisualActor->GetCrowdSpeed();
		const FVector Velocity = VisualActor->GetCrowdVelocity();
		const float HorizontalSpeedSq = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y;
		if (HorizontalSpeedSq > 0.0001f)
		{
			const float InvHorizontalSpeed = 1.0f / std::sqrt(HorizontalSpeedSq);
			const FVector MoveDir(Velocity.X * InvHorizontalSpeed, Velocity.Y * InvHorizontalSpeed, 0.0f);
			MoveForwardAmount = VisualActor->GetActorRotation().GetForwardVector().Dot(MoveDir);
			MoveRightAmount = VisualActor->GetActorRotation().GetRightVector().Dot(MoveDir);
		}
	}
}
