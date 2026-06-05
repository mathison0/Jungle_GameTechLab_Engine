#pragma once

#include "Component/ActorComponent.h"

#include "Source/Game/Musou/Combat/ComboComponent.generated.h"

// ============================================================
// UComboComponent — 좌클릭 콤보 체인 상태 관리 (초안)
//
// 실제 몽타주 재생은 Lua AnimInstance(player_anim.lua)가 담당하고,
// 이 컴포넌트는 콤보 단계/입력 버퍼/콤보 윈도우 타이밍만 관리한다.
//
// 예상 연동 흐름:
//   1) 입력 → TryAttack()
//      - 콤보 중이 아니면 1단 시작 (true 반환 → 호출측이 콤보 몽타주 재생)
//      - 콤보 중이면 다음 단계 입력 예약 (버퍼)
//   2) 몽타주 notify "ComboWindowOpen" → OpenComboWindow()
//      - 예약된 입력이 있으면 단계 전진 → ConsumeQueuedAdvance()가 true
//        → 호출측이 다음 섹션으로 jump_to_section
//   3) 몽타주 종료/notify "ComboEnd" → ResetCombo()
//
// TODO(확장): 단계별 데미지 배율, 히트박스 활성 구간, 강공격 캔슬 룰
// ============================================================
UCLASS()
class UComboComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UComboComponent() = default;
	~UComboComponent() override = default;

	// 공격 입력 진입점.
	// 반환 true  = 새 콤보 시작 (1단) → 호출측이 콤보 몽타주 재생
	// 반환 false = 콤보 진행 중 → 다음 단계 입력으로 버퍼링됨
	bool TryAttack();

	// 애님 notify 연동 — 콤보 윈도우 개폐.
	void OpenComboWindow();
	void CloseComboWindow();

	// 윈도우가 열렸을 때 예약 입력이 있으면 단계를 전진시키고 true.
	// 호출측(캐릭터/Lua)이 true를 받으면 다음 콤보 섹션을 재생한다.
	bool ConsumeQueuedAdvance();

	void ResetCombo();

	bool IsComboActive() const { return ComboStep > 0; }
	int32 GetComboStep() const { return ComboStep; }
	bool IsComboWindowOpen() const { return bWindowOpen; }

	// 콤보 최대 단수 (Combo Attack Ver. 1/2/3 기준 3단)
	UPROPERTY(Edit, Save, Category="Combo", DisplayName="Max Combo Steps")
	int32 MaxComboSteps = 3;

	// 윈도우 열리기 전 입력을 유지하는 시간(초)
	UPROPERTY(Edit, Save, Category="Combo", DisplayName="Input Buffer Time")
	float InputBufferTime = 0.3f;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	int32 ComboStep = 0;        // 0 = 비활성, 1..MaxComboSteps = 현재 단계
	bool bWindowOpen = false;   // 콤보 윈도우 개방 여부 (notify로 제어)
	bool bNextQueued = false;   // 다음 단계 입력 예약 여부
	float BufferRemaining = 0.0f;
};
