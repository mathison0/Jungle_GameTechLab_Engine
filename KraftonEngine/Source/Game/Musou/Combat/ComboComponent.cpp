#include "Game/Musou/Combat/ComboComponent.h"

bool UComboComponent::TryAttack()
{
	// 분기 피니셔 재생 중 — 체인 종료 확정, 라이트 입력 무시 (예약도 안 함).
	if (bBranchActive)
	{
		return false;
	}

	// 콤보 비활성 → 1단 시작.
	if (ComboStep == 0)
	{
		ComboStep = 1;
		bWindowOpen = false;
		bNextQueued = false;
		bHeavyQueued = false;
		BufferRemaining = 0.0f;
		return true;
	}

	// 콤보 진행 중 → 다음 단계 입력 예약 (버퍼). 분기 예약은 폐기 — 나중 입력 우선.
	if (ComboStep < MaxComboSteps)
	{
		bNextQueued = true;
		bHeavyQueued = false;
		BufferRemaining = InputBufferTime;
	}
	return false;
}

bool UComboComponent::TryHeavyBranch()
{
	// 콤보 비활성(호출측이 일반 강공격 처리) / 이미 분기 중 → 무시.
	if (ComboStep == 0 || bBranchActive)
	{
		return false;
	}

	// 분기 예약 (버퍼). 라이트 예약은 폐기 — 나중 입력 우선.
	bHeavyQueued = true;
	bNextQueued = false;
	BufferRemaining = InputBufferTime;
	return true;
}

void UComboComponent::OpenComboWindow()
{
	bWindowOpen = true;
}

void UComboComponent::CloseComboWindow()
{
	bWindowOpen = false;
	// 윈도우가 닫히면 미소비 예약 입력은 폐기 — 다음 스윙으로 새지 않게.
	bNextQueued = false;
	bHeavyQueued = false;
	BufferRemaining = 0.0f;
}

bool UComboComponent::ConsumeQueuedAdvance()
{
	if (!bWindowOpen || !bNextQueued || bBranchActive || ComboStep <= 0 || ComboStep >= MaxComboSteps)
	{
		return false;
	}

	++ComboStep;
	bNextQueued = false;
	bWindowOpen = false;
	BufferRemaining = 0.0f;
	return true;
}

int32 UComboComponent::ConsumeQueuedHeavyBranch()
{
	if (!bWindowOpen || !bHeavyQueued || bBranchActive || ComboStep <= 0)
	{
		return 0;
	}

	// 분기 확정 — 단수는 유지 (호출측이 단수별 피니셔 선택). 이후 전진/재분기 차단.
	bHeavyQueued = false;
	bWindowOpen = false;
	bBranchActive = true;
	BufferRemaining = 0.0f;
	return ComboStep;
}

void UComboComponent::ResetCombo()
{
	ComboStep = 0;
	bWindowOpen = false;
	bNextQueued = false;
	bHeavyQueued = false;
	bBranchActive = false;
	BufferRemaining = 0.0f;
}

void UComboComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 입력 버퍼 만료 — 윈도우가 열리기 전에 시간이 지나면 예약 취소.
	if ((bNextQueued || bHeavyQueued) && BufferRemaining > 0.0f)
	{
		BufferRemaining -= DeltaTime;
		if (BufferRemaining <= 0.0f)
		{
			bNextQueued = false;
			bHeavyQueued = false;
		}
	}
}
