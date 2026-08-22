#pragma once

#include "Animation/Notify/AnimNotifyState.h"

#include "Source/Game/Musou/Combat/AnimNotifyState_ComboWindow.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class UComboComponent;

// ============================================================
// UAnimNotifyState_ComboWindow — 콤보 입력 수용 구간
//
// 콤보 몽타주의 후딜 구간에 배치한다. 구간 동안 다음 단계 입력이
// 유효해지고(OpenComboWindow), 구간이 끝나면 미소비 예약은 폐기된다
// (CloseComboWindow). NotifyEnd는 몽타주 캔슬/전환 시에도 호출되므로
// 윈도우가 열린 채 새지 않는다.
//
// owner의 UComboComponent를 직접 호출 — lua를 거치지 않아
// 타이밍이 프레임 단위로 정확하다.
// ============================================================
UCLASS()
class UAnimNotifyState_ComboWindow : public UAnimNotifyState
{
public:
	GENERATED_BODY()
	UAnimNotifyState_ComboWindow() = default;
	~UAnimNotifyState_ComboWindow() override = default;

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

private:
	static UComboComponent* ResolveComboComponent(USkeletalMeshComponent* MeshComp);
};
