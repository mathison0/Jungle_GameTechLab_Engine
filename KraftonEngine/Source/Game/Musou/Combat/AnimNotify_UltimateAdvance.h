#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Game/Musou/Combat/AnimNotify_UltimateAdvance.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// ============================================================
// UAnimNotify_UltimateAdvance — 궁극기 다음 스텝 조기 전환 notify (one-shot)
//
// 백플립 몽타주의 "강타로 넘어갈" 프레임에 배치(또는 lua advance.trigger_frac 으로 주입).
// 발동 시 현재 궁극기 슬롯이 끝나길 기다리지 않고 즉시 다음 슬롯으로 cross-fade 한다.
// (노티파이가 안 박혀도 몽타주 blend-out 시 UpdateUltimateChain 이 폴백으로 전환.)
// ============================================================
UCLASS()
class UAnimNotify_UltimateAdvance : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_UltimateAdvance() = default;
	~UAnimNotify_UltimateAdvance() override = default;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
