#include "Game/Musou/Combat/ComboComponent.h"

bool UComboComponent::TryAttack()
{
	// 콤보 비활성 → 1단 시작.
	if (ComboStep == 0)
	{
		ComboStep = 1;
		bWindowOpen = false;
		bNextQueued = false;
		BufferRemaining = 0.0f;
		return true;
	}

	// 콤보 진행 중 → 다음 단계 입력 예약 (버퍼).
	if (ComboStep < MaxComboSteps)
	{
		bNextQueued = true;
		BufferRemaining = InputBufferTime;
	}
	return false;
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
	BufferRemaining = 0.0f;
}

bool UComboComponent::ConsumeQueuedAdvance()
{
	if (!bWindowOpen || !bNextQueued || ComboStep <= 0 || ComboStep >= MaxComboSteps)
	{
		return false;
	}

	++ComboStep;
	bNextQueued = false;
	bWindowOpen = false;
	BufferRemaining = 0.0f;
	return true;
}

void UComboComponent::ResetCombo()
{
	ComboStep = 0;
	bWindowOpen = false;
	bNextQueued = false;
	BufferRemaining = 0.0f;
}

void UComboComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 입력 버퍼 만료 — 윈도우가 열리기 전에 시간이 지나면 예약 취소.
	if (bNextQueued && BufferRemaining > 0.0f)
	{
		BufferRemaining -= DeltaTime;
		if (BufferRemaining <= 0.0f)
		{
			bNextQueued = false;
		}
	}
}
