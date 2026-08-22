#include "Game/Musou/UI/MusouMenuNavigator.h"

#include "Engine/Input/InputSystem.h"
#include "UI/UserWidget.h"

#include <algorithm>
#include <utility>

void FMusouMenuNavigator::SetWidget(UUserWidget* InWidget)
{
	Widget = InWidget;
	if (!Widget)
	{
		bHasSelection = false;
	}
}

void FMusouMenuNavigator::SetButtons(const char* const* InButtonIds, int32 InButtonCount)
{
	ButtonIds.clear();
	SelectedButtonIndex = 0;
	bHasSelection = false;

	if (!InButtonIds || InButtonCount <= 0)
	{
		return;
	}

	ButtonIds.reserve(static_cast<size_t>(InButtonCount));
	for (int32 ButtonIndex = 0; ButtonIndex < InButtonCount; ++ButtonIndex)
	{
		if (InButtonIds[ButtonIndex])
		{
			ButtonIds.emplace_back(InButtonIds[ButtonIndex]);
		}
	}
}

void FMusouMenuNavigator::BindHoverHandlers(TFunction<bool()> InCanSelectFromHover)
{
	CanSelectFromHover = std::move(InCanSelectFromHover);
	if (!Widget)
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < static_cast<int32>(ButtonIds.size()); ++ButtonIndex)
	{
		Widget->BindMouseOver(ButtonIds[static_cast<size_t>(ButtonIndex)], [this, ButtonIndex]()
		{
			if (CanSelectFromHover && !CanSelectFromHover())
			{
				return;
			}

			Select(ButtonIndex);
		});
	}
}

void FMusouMenuNavigator::Select(int32 ButtonIndex)
{
	if (ButtonIds.empty())
	{
		return;
	}

	SelectedButtonIndex = WrapIndex(ButtonIndex);
	bHasSelection = true;
	UpdateSelectionVisuals();
}

void FMusouMenuNavigator::EnsureSelection(int32 ButtonIndex)
{
	if (!bHasSelection)
	{
		Select(ButtonIndex);
		return;
	}

	UpdateSelectionVisuals();
}

void FMusouMenuNavigator::ClearSelection()
{
	if (IsReady())
	{
		for (const FString& ButtonId : ButtonIds)
		{
			Widget->SetClass(ButtonId, "selected", false);
		}
	}

	SelectedButtonIndex = 0;
	bHasSelection = false;
}

void FMusouMenuNavigator::MoveSelection(int32 Delta)
{
	if (ButtonIds.empty())
	{
		return;
	}

	if (!bHasSelection)
	{
		Select(0);
		return;
	}

	Select(SelectedButtonIndex + Delta);
}

void FMusouMenuNavigator::ExecuteSelection()
{
	if (ButtonIds.empty())
	{
		return;
	}

	EnsureSelection();
	if (!IsReady())
	{
		return;
	}

	Widget->Click(ButtonIds[static_cast<size_t>(SelectedButtonIndex)]);
}

bool FMusouMenuNavigator::HandleVerticalInput(InputSystem& Input, bool bSkipWhenTextInput)
{
	if (bSkipWhenTextInput && Input.IsGuiUsingTextInput())
	{
		return false;
	}

	bool bHandled = false;
	if (Input.GetKeyDown(VK_UP))
	{
		MoveSelection(-1);
		bHandled = true;
	}
	if (Input.GetKeyDown(VK_DOWN))
	{
		MoveSelection(1);
		bHandled = true;
	}
	if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
	{
		ExecuteSelection();
		bHandled = true;
	}

	return bHandled;
}

bool FMusouMenuNavigator::IsReady() const
{
	return Widget && Widget->IsDocumentLoaded() && !ButtonIds.empty();
}

int32 FMusouMenuNavigator::WrapIndex(int32 ButtonIndex) const
{
	const int32 ButtonCount = static_cast<int32>(ButtonIds.size());
	if (ButtonCount <= 0)
	{
		return 0;
	}

	return (ButtonIndex % ButtonCount + ButtonCount) % ButtonCount;
}

void FMusouMenuNavigator::UpdateSelectionVisuals()
{
	if (!IsReady())
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < static_cast<int32>(ButtonIds.size()); ++ButtonIndex)
	{
		Widget->SetClass(
			ButtonIds[static_cast<size_t>(ButtonIndex)],
			"selected",
			bHasSelection && ButtonIndex == SelectedButtonIndex);
	}
}
