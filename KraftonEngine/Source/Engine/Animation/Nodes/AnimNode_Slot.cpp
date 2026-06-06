#include "AnimNode_Slot.h"

#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/PoseContext.h"

#include <algorithm>

void FAnimNode_Slot::Initialize(const FAnimationInitializeContext& Context)
{
	OwnerAnimInstance = Context.AnimInstance;
	if (InputPose) InputPose->Initialize(Context);
}

void FAnimNode_Slot::OnBecomeRelevant(const FAnimationInitializeContext& Context)
{
	if (InputPose) InputPose->OnBecomeRelevant(Context);
}

void FAnimNode_Slot::Update(const FAnimationUpdateContext& Context)
{
	if (InputPose)
	{
		InputPose->Update(Context);
		InputLastRM = InputPose->GetLastRootMotionDelta();
	}
	else
	{
		InputLastRM = FTransform();
	}

	if (!OwnerAnimInstance) return;

	// Montage RM 을 현재 누적값과 weight 로 lerp — pose 합성과 동일한 weight 정책.
	auto BlendMontageRM = [this](UAnimMontageInstance* MI)
	{
		const float W = MI->GetBlendWeight();
		if (W <= 0.0f) return;
		const FTransform& MontageRM = MI->GetLastRootMotionDelta();
		InputLastRM.Location = InputLastRM.Location * (1.0f - W) + MontageRM.Location * W;
		InputLastRM.Rotation = FQuat::Slerp(InputLastRM.Rotation.GetNormalized(),
		                                    MontageRM.Rotation.GetNormalized(), W).GetNormalized();
	};

	// 1) Cross-fade 로 blend-out 중인 (교체된) montage 들 — tick + RM 합성 + Inactive 정리.
	//    현재 montage 보다 먼저 합성해 새 montage 의 weight 가 마지막에 지배하도록.
	if (TArray<UAnimMontageInstance*>* Fading = OwnerAnimInstance->GetFadingMontageInstancesForSlot(SlotName))
	{
		for (size_t i = 0; i < Fading->size(); )
		{
			UAnimMontageInstance* FMI = (*Fading)[i];
			if (!FMI || !FMI->IsActive())
			{
				Fading->erase(Fading->begin() + i);
				continue;
			}
			FMI->Tick(Context.DeltaSeconds, OwnerAnimInstance);
			if (!FMI->IsActive())
			{
				Fading->erase(Fading->begin() + i);
				continue;
			}
			BlendMontageRM(FMI);
			++i;
		}
	}

	// 2) 현재 활성 montage — 자기 slot 의 tick + RM 합성. Slot 이 단일 책임자.
	//    Montage 의 LastRM 은 raw delta (W 곱 안 함) 라 Slot 측에서 lerp.
	UAnimMontageInstance* MI = OwnerAnimInstance->GetMontageInstanceForSlot(SlotName);
	if (MI && MI->IsActive())
	{
		MI->Tick(Context.DeltaSeconds, OwnerAnimInstance);
		BlendMontageRM(MI);
	}
}

float FAnimNode_Slot::GetEffectiveBlendWeight() const
{
	if (!OwnerAnimInstance) return 0.0f;

	float MaxW = 0.0f;
	UAnimMontageInstance* MI = OwnerAnimInstance->GetMontageInstanceForSlot(SlotName);
	if (MI && MI->IsActive())
	{
		MaxW = MI->GetBlendWeight();
	}

	// Cross-fade 중인 (교체된) montage 도 가시 weight 에 포함 — 교체 직후 frame 에
	// current W≈0 이라고 0 을 반환하면 LayeredBlendPerBone 류 사용처가 한 프레임 출렁인다.
	const UAnimInstance* ConstOwner = OwnerAnimInstance;
	if (const TArray<UAnimMontageInstance*>* Fading = ConstOwner->GetFadingMontageInstancesForSlot(SlotName))
	{
		for (UAnimMontageInstance* FMI : *Fading)
		{
			if (FMI && FMI->IsActive())
			{
				MaxW = std::max(MaxW, FMI->GetBlendWeight());
			}
		}
	}
	return MaxW;
}

void FAnimNode_Slot::Evaluate(FPoseContext& Output)
{
	// 1) InputPose 평가 — base pose 가 Output 에 들어감. 없으면 ref pose fallback.
	if (InputPose)
	{
		InputPose->Evaluate(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}

	if (!OwnerAnimInstance) return;

	// Montage pose 평가 후 BlendWeight 로 lerp. BlendTwoPosesTogether 가 in-place 안전
	// (Output == A 케이스 OK).
	auto BlendMontagePose = [&Output](UAnimMontageInstance* MI)
	{
		if (!MI || !MI->IsActive()) return;
		const float Weight = MI->GetBlendWeight();
		if (Weight <= 0.0f) return;

		FPoseContext MontagePose;
		MontagePose.SkeletalMesh = Output.SkeletalMesh;
		MontagePose.ResetToRefPose();
		MI->EvaluateMontagePose(MontagePose);

		if (Weight >= 1.0f)
		{
			// 완전 montage — 이전 합성 결과 무시.
			Output = MontagePose;
		}
		else
		{
			FAnimationRuntime::BlendTwoPosesTogether(Output, MontagePose, Weight, Output);
		}
	};

	// 2) Cross-fade 로 blend-out 중인 montage 들 먼저 — 새(현재) montage 가 마지막에
	//    합성되어 지배권을 가진다. 교체 직후 frame: fading W≈1 + current W≈0 → 이전
	//    포즈가 유지되며 부드럽게 전환.
	if (TArray<UAnimMontageInstance*>* Fading = OwnerAnimInstance->GetFadingMontageInstancesForSlot(SlotName))
	{
		for (UAnimMontageInstance* FMI : *Fading)
		{
			BlendMontagePose(FMI);
		}
	}

	// 3) 현재 활성 montage.
	BlendMontagePose(OwnerAnimInstance->GetMontageInstanceForSlot(SlotName));
}
