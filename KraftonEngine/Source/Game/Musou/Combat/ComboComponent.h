#pragma once

#include "Component/ActorComponent.h"

#include "Source/Game/Musou/Combat/ComboComponent.generated.h"

// ============================================================
// UComboComponent — 좌클릭 콤보 체인(공격 단계) 상태 관리
//
// ※ AMusouGameState의 Combo(킬 스트릭)와는 별개 — 여기는 "공격 몇 단째"다.
//
// 실제 몽타주 재생은 Lua AnimInstance(player_anim.lua)가 담당하고,
// 이 컴포넌트는 콤보 단계/입력 버퍼/콤보 윈도우 타이밍만 관리한다.
// 단계별 히트 판정은 각 몽타주의 AnimNotify_MusouAttack(comboN)이
// 기존 공격 이벤트 파이프라인으로 처리 — 이 컴포넌트와 무관.
//
// 연동 흐름:
//   1) 입력(lua) → TryAttack()
//      - 콤보 중이 아니면 1단 시작 (true 반환 → lua가 1단 몽타주 재생)
//      - 콤보 중이면 다음 단계 입력 예약 (버퍼)
//   2) 몽타주 후딜의 AnimNotifyState_ComboWindow 구간(C++ 직결)
//      - Begin → OpenComboWindow() / End → CloseComboWindow()
//   3) lua Tick: ConsumeQueuedAdvance() == true → 다음 단계 몽타주 재생
//   4) lua Tick: 몽타주 미재생 + 콤보 활성 → ResetCombo() (체인 끊김/완주)
//
// 강공격 분기 (무쌍 차지어택식 □..△):
//   콤보 진행 중 강공격 입력 → TryHeavyBranch() 로 예약 (라이트 예약과 상호 배타,
//   나중 입력 우선). 윈도우에서 ConsumeQueuedHeavyBranch() 가 현재 단수를 반환하면
//   호출측이 단수별 분기 피니셔를 재생. 분기 후엔 체인 종료 — 추가 전진/분기 불가,
//   분기 몽타주 종료 시 기존 리셋 경로(몽타주 미재생 → ResetCombo)로 정리된다.
//
// TODO(확장): 단계별 이동 잠금
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

	// 강공격 분기 입력 진입점 — 콤보 진행 중에만 유효 (비활성/분기 중엔 false).
	// true = 분기 예약됨. 라이트 예약(bNextQueued)은 폐기 — 나중 입력 우선.
	bool TryHeavyBranch();

	// 윈도우가 열렸을 때 분기 예약이 있으면 분기 상태로 전환하고 현재 단수 반환 (0 = 없음).
	// 호출측이 단수별 분기 피니셔 몽타주를 재생한다. 라이트 전진보다 먼저 폴링할 것.
	int32 ConsumeQueuedHeavyBranch();

	void ResetCombo();

	// 활성 체인 길이 설정 — 진입 컨텍스트(제자리/이동/공중)별로 체인 단수가 다르다.
	// 새 콤보 시작 직전(TryAttack 전)에 호출. 진행 중 변경하면 단수 판정이 흔들리므로 금지.
	void SetMaxComboSteps(int32 InNumSteps) { MaxComboSteps = (InNumSteps < 1) ? 1 : InNumSteps; }

	bool IsComboActive() const { return ComboStep > 0; }
	int32 GetComboStep() const { return ComboStep; }
	bool IsComboWindowOpen() const { return bWindowOpen; }
	bool IsBranchActive() const { return bBranchActive; }

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
	bool bHeavyQueued = false;  // 강공격 분기 예약 여부 (bNextQueued 와 상호 배타)
	bool bBranchActive = false; // 분기 피니셔 재생 중 — 추가 전진/분기 차단
	float BufferRemaining = 0.0f;
};
