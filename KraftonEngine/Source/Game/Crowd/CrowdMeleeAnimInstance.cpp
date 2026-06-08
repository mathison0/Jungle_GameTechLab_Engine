#include "Game/Crowd/CrowdMeleeAnimInstance.h"

#include "Animation/AnimationManager.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/StateMachine/AnimState.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Game/Crowd/CrowdUnitVisualActor.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float CrowdMeleeBlendTime = 0.12f;
	constexpr float CrowdMeleeDirectionDeadZone = 0.20f;
	constexpr float CrowdMeleeAnimMinStateHoldTime = 0.18f;
	constexpr float CrowdMeleeAnimDirectionSwitchMargin = 0.18f;
	constexpr float CrowdMeleeAnimRunEnterSpeed = 5.25f;
	constexpr float CrowdMeleeAnimRunExitSpeed = 4.75f;

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
	const FName CrowdMeleeGettingUpState("GettingUp");
	const FName CrowdMeleeDeadState("Dead");

	const char* ToCrowdUnitStateString(EUnitState State)
	{
		switch (State)
		{
		case EUnitState::Idle:
			return "Idle";
		case EUnitState::Move:
			return "Move";
		case EUnitState::Chase:
			return "Chase";
		case EUnitState::CircleAround:
			return "CircleAround";
		case EUnitState::Attack:
			return "Attack";
		case EUnitState::Hit:
			return "Hit";
		case EUnitState::KnockDown:
			return "KnockDown";
		case EUnitState::Dead:
			return "Dead";
		default:
			return "Unknown";
		}
	}

	FString SequencePathForMeleeState(const FCrowdMeleeAnimationSet& AnimationSet, FName StateName)
	{
		FString Path = "None";
		if (StateName == CrowdMeleeIdleState)
		{
			Path = AnimationSet.IdleSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeWalkForwardState)
		{
			Path = AnimationSet.WalkForwardSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeWalkBackwardState)
		{
			Path = AnimationSet.WalkBackwardSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeRunForwardState)
		{
			Path = AnimationSet.RunForwardSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeRunBackwardState)
		{
			Path = AnimationSet.RunBackwardSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeStrafeWalkLeftState)
		{
			Path = AnimationSet.StrafeWalkLeftSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeStrafeWalkRightState)
		{
			Path = AnimationSet.StrafeWalkRightSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeStrafeRunLeftState)
		{
			Path = AnimationSet.StrafeRunLeftSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeStrafeRunRightState)
		{
			Path = AnimationSet.StrafeRunRightSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeAttackState)
		{
			Path = AnimationSet.AttackSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeHitState)
		{
			Path = AnimationSet.HitSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeKnockDownState)
		{
			Path = AnimationSet.KnockDownSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeGettingUpState)
		{
			Path = AnimationSet.GettingUpSequencePath.ToString();
		}
		else if (StateName == CrowdMeleeDeadState)
		{
			Path = AnimationSet.DeadSequencePath.ToString();
		}

		return Path.empty() ? FString("None") : Path;
	}

	enum class ECrowdMeleeAnimDirection
	{
		Forward,
		Backward,
		Left,
		Right
	};

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

	bool IsImmediateMeleeState(FName StateName)
	{
		return StateName == CrowdMeleeDeadState
			|| StateName == CrowdMeleeKnockDownState
			|| StateName == CrowdMeleeGettingUpState
			|| StateName == CrowdMeleeHitState
			|| StateName == CrowdMeleeAttackState
			|| StateName == CrowdMeleeIdleState;
	}

	bool IsRunMeleeState(FName StateName)
	{
		return StateName == CrowdMeleeRunForwardState
			|| StateName == CrowdMeleeRunBackwardState
			|| StateName == CrowdMeleeStrafeRunLeftState
			|| StateName == CrowdMeleeStrafeRunRightState;
	}

	bool IsStrafeMeleeState(FName StateName)
	{
		return StateName == CrowdMeleeStrafeWalkLeftState
			|| StateName == CrowdMeleeStrafeWalkRightState
			|| StateName == CrowdMeleeStrafeRunLeftState
			|| StateName == CrowdMeleeStrafeRunRightState;
	}

	bool IsLocomotionMeleeState(FName StateName)
	{
		return StateName == CrowdMeleeWalkForwardState
			|| StateName == CrowdMeleeWalkBackwardState
			|| StateName == CrowdMeleeRunForwardState
			|| StateName == CrowdMeleeRunBackwardState
			|| StateName == CrowdMeleeStrafeWalkLeftState
			|| StateName == CrowdMeleeStrafeWalkRightState
			|| StateName == CrowdMeleeStrafeRunLeftState
			|| StateName == CrowdMeleeStrafeRunRightState;
	}

	ECrowdMeleeAnimDirection DirectionFromState(FName StateName)
	{
		if (StateName == CrowdMeleeWalkBackwardState || StateName == CrowdMeleeRunBackwardState)
		{
			return ECrowdMeleeAnimDirection::Backward;
		}
		if (StateName == CrowdMeleeStrafeWalkLeftState || StateName == CrowdMeleeStrafeRunLeftState)
		{
			return ECrowdMeleeAnimDirection::Left;
		}
		if (StateName == CrowdMeleeStrafeWalkRightState || StateName == CrowdMeleeStrafeRunRightState)
		{
			return ECrowdMeleeAnimDirection::Right;
		}
		return ECrowdMeleeAnimDirection::Forward;
	}

	float DirectionAxisStrength(ECrowdMeleeAnimDirection Direction, float ForwardAmount, float RightAmount)
	{
		switch (Direction)
		{
		case ECrowdMeleeAnimDirection::Backward:
			return (std::max)(-ForwardAmount, 0.0f);
		case ECrowdMeleeAnimDirection::Left:
			return (std::max)(-RightAmount, 0.0f);
		case ECrowdMeleeAnimDirection::Right:
			return (std::max)(RightAmount, 0.0f);
		case ECrowdMeleeAnimDirection::Forward:
		default:
			return (std::max)(ForwardAmount, 0.0f);
		}
	}

	ECrowdMeleeAnimDirection SelectRawDirection(float ForwardAmount, float RightAmount)
	{
		const float AbsForward = std::abs(ForwardAmount);
		const float AbsRight = std::abs(RightAmount);
		if (AbsRight > AbsForward && AbsRight > CrowdMeleeDirectionDeadZone)
		{
			return RightAmount > 0.0f ? ECrowdMeleeAnimDirection::Right : ECrowdMeleeAnimDirection::Left;
		}

		if (ForwardAmount < -CrowdMeleeDirectionDeadZone)
		{
			return ECrowdMeleeAnimDirection::Backward;
		}

		return ECrowdMeleeAnimDirection::Forward;
	}

	ECrowdMeleeAnimDirection SelectStableDirection(
		float ForwardAmount,
		float RightAmount,
		FName CurrentStateName)
	{
		const ECrowdMeleeAnimDirection DesiredDirection = SelectRawDirection(ForwardAmount, RightAmount);
		if (!IsLocomotionMeleeState(CurrentStateName))
		{
			return DesiredDirection;
		}

		const ECrowdMeleeAnimDirection CurrentDirection = DirectionFromState(CurrentStateName);
		if (CurrentDirection == DesiredDirection)
		{
			return DesiredDirection;
		}

		const float CurrentStrength = DirectionAxisStrength(CurrentDirection, ForwardAmount, RightAmount);
		const float DesiredStrength = DirectionAxisStrength(DesiredDirection, ForwardAmount, RightAmount);
		if (CurrentStrength + CrowdMeleeAnimDirectionSwitchMargin >= DesiredStrength)
		{
			return CurrentDirection;
		}

		return DesiredDirection;
	}

	ECrowdMeleeAnimDirection SelectRawForwardBackwardDirection(float ForwardAmount)
	{
		return ForwardAmount < -CrowdMeleeDirectionDeadZone
			? ECrowdMeleeAnimDirection::Backward
			: ECrowdMeleeAnimDirection::Forward;
	}

	ECrowdMeleeAnimDirection SelectStableForwardBackwardDirection(
		float ForwardAmount,
		FName CurrentStateName)
	{
		const ECrowdMeleeAnimDirection DesiredDirection = SelectRawForwardBackwardDirection(ForwardAmount);
		if (!IsLocomotionMeleeState(CurrentStateName) || IsStrafeMeleeState(CurrentStateName))
		{
			return DesiredDirection;
		}

		const ECrowdMeleeAnimDirection CurrentDirection = DirectionFromState(CurrentStateName);
		if (CurrentDirection == DesiredDirection)
		{
			return DesiredDirection;
		}

		const float CurrentStrength = DirectionAxisStrength(CurrentDirection, ForwardAmount, 0.0f);
		const float DesiredStrength = DirectionAxisStrength(DesiredDirection, ForwardAmount, 0.0f);
		if (CurrentStrength + CrowdMeleeAnimDirectionSwitchMargin >= DesiredStrength)
		{
			return CurrentDirection;
		}

		return DesiredDirection;
	}

	FName StateFromDirection(ECrowdMeleeAnimDirection Direction, bool bUseRun)
	{
		switch (Direction)
		{
		case ECrowdMeleeAnimDirection::Backward:
			return bUseRun ? CrowdMeleeRunBackwardState : CrowdMeleeWalkBackwardState;
		case ECrowdMeleeAnimDirection::Left:
			return bUseRun ? CrowdMeleeStrafeRunLeftState : CrowdMeleeStrafeWalkLeftState;
		case ECrowdMeleeAnimDirection::Right:
			return bUseRun ? CrowdMeleeStrafeRunRightState : CrowdMeleeStrafeWalkRightState;
		case ECrowdMeleeAnimDirection::Forward:
		default:
			return bUseRun ? CrowdMeleeRunForwardState : CrowdMeleeWalkForwardState;
		}
	}

	bool HasLocomotionSpeed(const UCrowdUnitAnimInstance& AnimInstance)
	{
		const float IdleSpeedThreshold = (std::max)(AnimInstance.GetCrowdLocomotionIdleSpeedThreshold(), 0.0f);
		return AnimInstance.GetCrowdSpeed() > IdleSpeedThreshold;
	}

	bool ShouldUseRunForSpeed(const UCrowdUnitAnimInstance& AnimInstance, FName CurrentStateName)
	{
		const bool bWasRun = IsRunMeleeState(CurrentStateName);
		return bWasRun
			? AnimInstance.GetCrowdSpeed() >= CrowdMeleeAnimRunExitSpeed
			: AnimInstance.GetCrowdSpeed() >= CrowdMeleeAnimRunEnterSpeed;
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
	StableMeleeStateName = FName::None;
	StableMeleeStateElapsedTime = 0.0f;
	LastLoggedMeleeStateName = FName::None;
	LastLoggedCrowdState = EUnitState::Idle;
	bHasLoggedMeleeState = false;

	Super::NativeInitializeAnimation();
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

	BuildMeleeGraph();
}

void UCrowdMeleeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UpdateStableMeleeState(DeltaSeconds);
	LogMeleeAnimStateIfChanged();
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
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeGettingUpState, LoadSequence(AnimationSet.GettingUpSequencePath), false));
	StateMachine->RegisterState(MakeCrowdMeleeState(this, CrowdMeleeDeadState, LoadSequence(AnimationSet.DeadSequencePath), false));

	StateMachine->RegisterTransition({ FName::None, CrowdMeleeDeadState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeDeadState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeKnockDownState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeKnockDownState); }, CrowdMeleeBlendTime });
	StateMachine->RegisterTransition({ FName::None, CrowdMeleeGettingUpState, [this](UAnimInstance*) { return WantsMeleeState(CrowdMeleeGettingUpState); }, CrowdMeleeBlendTime });
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
	return StableMeleeStateName != FName::None ? StableMeleeStateName : ComputeDesiredMeleeStateName();
}

FName UCrowdMeleeAnimInstance::ComputeDesiredMeleeStateName() const
{
	switch (GetLastCrowdState())
	{
	case EUnitState::Dead:
		return CrowdMeleeDeadState;
	case EUnitState::KnockDown:
		return IsCrowdKnockDownGettingUp() ? CrowdMeleeGettingUpState : CrowdMeleeKnockDownState;
	case EUnitState::Hit:
		return CrowdMeleeHitState;
	case EUnitState::Attack:
		return CrowdMeleeAttackState;
	case EUnitState::Chase:
	{
		if (!HasLocomotionSpeed(*this))
		{
			return CrowdMeleeIdleState;
		}

		return StateFromDirection(
			SelectStableForwardBackwardDirection(GetCrowdMoveForwardAmount(), StableMeleeStateName),
			ShouldUseRunForSpeed(*this, StableMeleeStateName));
	}
	case EUnitState::CircleAround:
	{
		if (!HasLocomotionSpeed(*this))
		{
			return CrowdMeleeIdleState;
		}

		return StateFromDirection(
			SelectStableDirection(GetCrowdMoveForwardAmount(), GetCrowdMoveRightAmount(), StableMeleeStateName),
			ShouldUseRunForSpeed(*this, StableMeleeStateName));
	}
	case EUnitState::Move:
	{
		if (!HasLocomotionSpeed(*this))
		{
			return CrowdMeleeIdleState;
		}

		return StateFromDirection(
			SelectStableForwardBackwardDirection(GetCrowdMoveForwardAmount(), StableMeleeStateName),
			ShouldUseRunForSpeed(*this, StableMeleeStateName));
	}
	case EUnitState::Idle:
	default:
		return CrowdMeleeIdleState;
	}
}

void UCrowdMeleeAnimInstance::UpdateStableMeleeState(float DeltaSeconds)
{
	const FName DesiredStateName = ComputeDesiredMeleeStateName();
	if (StableMeleeStateName == FName::None)
	{
		StableMeleeStateName = DesiredStateName;
		StableMeleeStateElapsedTime = 0.0f;
		return;
	}

	StableMeleeStateElapsedTime += (std::max)(DeltaSeconds, 0.0f);
	if (StableMeleeStateName == DesiredStateName)
	{
		return;
	}

	const bool bLeavingCircleAroundStrafe = GetLastCrowdState() != EUnitState::CircleAround
		&& IsStrafeMeleeState(StableMeleeStateName);
	if (IsImmediateMeleeState(DesiredStateName)
		|| bLeavingCircleAroundStrafe
		|| !IsLocomotionMeleeState(StableMeleeStateName)
		|| StableMeleeStateElapsedTime >= CrowdMeleeAnimMinStateHoldTime)
	{
		StableMeleeStateName = DesiredStateName;
		StableMeleeStateElapsedTime = 0.0f;
	}
}

void UCrowdMeleeAnimInstance::LogMeleeAnimStateIfChanged()
{
	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	ACrowdUnitVisualActor* VisualActor = MeshComp ? Cast<ACrowdUnitVisualActor>(MeshComp->GetOwner()) : nullptr;
	if (!VisualActor || !VisualActor->IsVisualActive() || !VisualActor->ShouldLogCrowdAnimationState())
	{
		return;
	}

	const EUnitState CrowdState = GetLastCrowdState();
	const FName MeleeStateName = GetDesiredMeleeStateName();
	if (bHasLoggedMeleeState
		&& LastLoggedCrowdState == CrowdState
		&& LastLoggedMeleeStateName == MeleeStateName)
	{
		return;
	}

	LastLoggedCrowdState = CrowdState;
	LastLoggedMeleeStateName = MeleeStateName;
	bHasLoggedMeleeState = true;

	const FUnitHandle UnitHandle = VisualActor->GetUnitHandle();
	const FString MeleeStateString = MeleeStateName.ToString();
	const FString SequencePath = SequencePathForMeleeState(AnimationSet, MeleeStateName);
	UE_LOG(
		"[CrowdUnitAnim] Unit=%u:%u CrowdState=%s AnimState=%s Sequence=%s Speed=%.2f Forward=%.2f Right=%.2f",
		UnitHandle.Index,
		UnitHandle.Generation,
		ToCrowdUnitStateString(CrowdState),
		MeleeStateString.c_str(),
		SequencePath.c_str(),
		GetCrowdSpeed(),
		GetCrowdMoveForwardAmount(),
		GetCrowdMoveRightAmount());
}

bool UCrowdMeleeAnimInstance::WantsMeleeState(FName StateName) const
{
	return GetDesiredMeleeStateName() == StateName;
}
