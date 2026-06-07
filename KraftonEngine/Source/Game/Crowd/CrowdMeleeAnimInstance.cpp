#include "Game/Crowd/CrowdMeleeAnimInstance.h"

#include "Animation/AnimationManager.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/StateMachine/AnimState.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Object/Object.h"

#include <cmath>

namespace
{
	constexpr float CrowdMeleeBlendTime = 0.12f;
	constexpr float CrowdMeleeRunSpeedThreshold = 5.0f;
	constexpr float CrowdMeleeDirectionDeadZone = 0.20f;

	const FName CrowdMeleeIdleState("Idle");
	const FName CrowdMeleeWalkForwardState("WalkForward");
	const FName CrowdMeleeWalkBackwardState("WalkBackward");
	const FName CrowdMeleeRunForwardState("RunForward");
	const FName CrowdMeleeRunBackwardState("RunBackward");
	const FName CrowdMeleeStrafeWalkLeftState("StrafeWalkLeft");
	const FName CrowdMeleeStrafeWalkRightState("StrafeWalkRight");
	const FName CrowdMeleeStrafeRunLeftState("StrafeRunLeft");
	const FName CrowdMeleeStrafeRunRightState("StrafeRunRight");
	const FName CrowdMeleeAttackState("Attack");
	const FName CrowdMeleeHitState("Hit");
	const FName CrowdMeleeKnockDownState("KnockDown");
	const FName CrowdMeleeDeadState("Dead");

	UAnimState* MakeCrowdMeleeState(
		UObject* Outer,
		FName StateName,
		UAnimSequenceBase* Sequence,
		bool bLooping)
	{
		UAnimState* State = UObjectManager::Get().CreateObject<UAnimState>(Outer);
		State->StateName = StateName;
		State->Sequence = Sequence;
		State->PlayRate = 1.0f;
		State->bLooping = bLooping;
		return State;
	}

	bool IsRunSpeed(float Speed)
	{
		return Speed >= CrowdMeleeRunSpeedThreshold;
	}

	FName SelectDirectionalLocomotionState(float ForwardAmount, float RightAmount, bool bUseRun)
	{
		const float AbsForward = std::abs(ForwardAmount);
		const float AbsRight = std::abs(RightAmount);
		if (AbsRight > AbsForward && AbsRight > CrowdMeleeDirectionDeadZone)
		{
			if (RightAmount > 0.0f)
			{
				return bUseRun ? CrowdMeleeStrafeRunRightState : CrowdMeleeStrafeWalkRightState;
			}

			return bUseRun ? CrowdMeleeStrafeRunLeftState : CrowdMeleeStrafeWalkLeftState;
		}

		if (ForwardAmount < -CrowdMeleeDirectionDeadZone)
		{
			return bUseRun ? CrowdMeleeRunBackwardState : CrowdMeleeWalkBackwardState;
		}

		return bUseRun ? CrowdMeleeRunForwardState : CrowdMeleeWalkForwardState;
	}
}

UCrowdMeleeAnimInstance::UCrowdMeleeAnimInstance()
{
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UCrowdMeleeAnimInstance::NativeInitializeAnimation()
{
	SetRootNode(nullptr);
	OwnedNodes.clear();

	Super::NativeInitializeAnimation();
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

	BuildMeleeGraph();
}

void UCrowdMeleeAnimInstance::SetMeleeAnimationSet(const FCrowdMeleeAnimationSet& InAnimationSet)
{
	if (AnimationSet == InAnimationSet)
	{
		return;
	}

	AnimationSet = InAnimationSet;
	if (GetOwningComponent())
	{
		NativeInitializeAnimation();
	}
}

void UCrowdMeleeAnimInstance::BuildMeleeGraph()
{
	FAnimNode_StateMachine* StateMachine = MakeNode<FAnimNode_StateMachine>();
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeIdleState, LoadSequence(AnimationSet.IdleSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeWalkForwardState, LoadSequence(AnimationSet.WalkForwardSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeWalkBackwardState, LoadSequence(AnimationSet.WalkBackwardSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeRunForwardState, LoadSequence(AnimationSet.RunForwardSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeRunBackwardState, LoadSequence(AnimationSet.RunBackwardSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeStrafeWalkLeftState, LoadSequence(AnimationSet.StrafeWalkLeftSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeStrafeWalkRightState, LoadSequence(AnimationSet.StrafeWalkRightSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeStrafeRunLeftState, LoadSequence(AnimationSet.StrafeRunLeftSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeStrafeRunRightState, LoadSequence(AnimationSet.StrafeRunRightSequencePath), true));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeAttackState, LoadSequence(AnimationSet.AttackSequencePath), false));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeHitState, LoadSequence(AnimationSet.HitSequencePath), false));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeKnockDownState, LoadSequence(AnimationSet.KnockDownSequencePath), false));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeDeadState, LoadSequence(AnimationSet.DeadSequencePath), false));

	StateMachine->RegisterTransition({ FName::None, CrowdMeleeDeadState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeDeadState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeKnockDownState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeKnockDownState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeHitState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeHitState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeAttackState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeAttackState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeRunForwardState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeRunForwardState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeRunBackwardState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeRunBackwardState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeStrafeRunLeftState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeStrafeRunLeftState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeStrafeRunRightState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeStrafeRunRightState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeWalkForwardState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeWalkForwardState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeWalkBackwardState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeWalkBackwardState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeStrafeWalkLeftState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeStrafeWalkLeftState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeStrafeWalkRightState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeStrafeWalkRightState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeIdleState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeIdleState); }, CrowdMeleeBlendTime });
	StateMachine->SetInitialState(CrowdMeleeIdleState);

	FAnimNode_Slot* DefaultSlot = MakeNode<FAnimNode_Slot>();
	DefaultSlot->SlotName = UAnimInstance::DefaultMontageSlot;
	DefaultSlot->InputPose = StateMachine;
	SetRootNode(DefaultSlot);
}

UAnimSequenceBase* UCrowdMeleeAnimInstance::LoadSequence(const FSoftObjectPtr& SequencePath) const
{
	const FString Path = SequencePath.ToString();
	if (Path.empty() || Path == "None")
	{
		return nullptr;
	}

	UAnimSequence* Sequence = FAnimationManager::Get().LoadAnimation(Path);
	if (!Sequence)
	{
		UE_LOG("[CrowdMeleeAnim] Failed to load animation sequence: %s", Path.c_str());
		return nullptr;
	}

	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	if (MeshComp && !MeshComp->CanUseAnimation(Sequence))
	{
		return nullptr;
	}

	return Sequence;
}

FName UCrowdMeleeAnimInstance::GetDesiredMeleeStateName() const
{
	switch (GetLastCrowdState())
	{
	case EUnitState::Dead:
		return CrowdMeleeDeadState;
	case EUnitState::KnockDown:
		return CrowdMeleeKnockDownState;
	case EUnitState::Hit:
		return CrowdMeleeHitState;
	case EUnitState::Attack:
		return CrowdMeleeAttackState;
	case EUnitState::Chase:
		return SelectDirectionalLocomotionState(GetCrowdMoveForwardAmount(), GetCrowdMoveRightAmount(), true);
	case EUnitState::CircleAround:
		return SelectDirectionalLocomotionState(GetCrowdMoveForwardAmount(), GetCrowdMoveRightAmount(), IsRunSpeed(GetCrowdSpeed()));
	case EUnitState::Move:
		return SelectDirectionalLocomotionState(GetCrowdMoveForwardAmount(), GetCrowdMoveRightAmount(), IsRunSpeed(GetCrowdSpeed()));
	case EUnitState::Idle:
	default:
		return CrowdMeleeIdleState;
	}
}

bool UCrowdMeleeAnimInstance::WantsMeleeState(FName StateName) const
{
	return GetDesiredMeleeStateName() == StateName;
}
