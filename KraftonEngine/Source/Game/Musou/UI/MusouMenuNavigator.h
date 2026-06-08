#pragma once

#include "Core/Types/CoreTypes.h"

class InputSystem;
class UUserWidget;

// RML 버튼 묶음의 키보드 포커스/실행을 담당하는 작은 helper.
// GameMode는 "지금 어떤 메뉴가 활성인가"만 판단하고, 선택 인덱스와 selected class 관리는 여기로 모은다.
class FMusouMenuNavigator
{
public:
	void SetWidget(UUserWidget* InWidget);
	void SetButtons(const char* const* InButtonIds, int32 InButtonCount);
	void BindHoverHandlers(TFunction<bool()> InCanSelectFromHover);

	void Select(int32 ButtonIndex);
	void EnsureSelection(int32 ButtonIndex = 0);
	void ClearSelection();
	void MoveSelection(int32 Delta);
	void ExecuteSelection();

	// 세로 버튼 메뉴의 공통 조작: 위/아래 이동, Enter/Space 실행.
	// 이름 입력창처럼 RML input이 키를 우선 처리해야 하는 경우 bSkipWhenTextInput을 켠다.
	bool HandleVerticalInput(InputSystem& Input, bool bSkipWhenTextInput = false);

	bool HasSelection() const { return bHasSelection; }

private:
	bool IsReady() const;
	int32 WrapIndex(int32 ButtonIndex) const;
	void UpdateSelectionVisuals();

	UUserWidget* Widget = nullptr;
	TArray<FString> ButtonIds;
	TFunction<bool()> CanSelectFromHover;
	int32 SelectedButtonIndex = 0;
	bool bHasSelection = false;
};
