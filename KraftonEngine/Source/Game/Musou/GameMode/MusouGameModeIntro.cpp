#include "Game/Musou/GameMode/MusouGameModeIntro.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/Score/MusouScoreboard.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>  // PostQuitMessage

namespace
{
	constexpr const char* IntroButtonIds[] = {
		"start-button",
		"settings-button",
		"score-button",
		"exit-button",
	};

	constexpr int32 IntroButtonCount = 4;
	constexpr int32 IntroScoreboardPageSize = 10;
}

AMusouGameModeIntro::AMusouGameModeIntro()
{
}

void AMusouGameModeIntro::StartMatch()
{
	AGameModeBase::StartMatch();

	if (!IntroWidget)
	{
		IntroWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/Intro.rml");
		if (IntroWidget)
		{
			IntroWidget->BindClick("start-button", []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Play");
				}
			});

			IntroWidget->BindClick("score-button", [this]()
			{
				ShowScoreboard();
			});

			IntroWidget->BindClick("scoreboard-prev-button", [this]()
			{
				ShowPreviousScoreboardPage();
			});

			IntroWidget->BindClick("scoreboard-next-button", [this]()
			{
				ShowNextScoreboardPage();
			});

			IntroWidget->BindClick("scoreboard-close-button", [this]()
			{
				HideScoreboard();
			});

			IntroWidget->BindClick("exit-button", []()
			{
				UE_LOG("[MusouGameModeIntro] Exit requested from intro UI");
				PostQuitMessage(0);
			});

			for (int32 ButtonIndex = 0; ButtonIndex < IntroButtonCount; ++ButtonIndex)
			{
				IntroWidget->BindMouseOver(IntroButtonIds[ButtonIndex], [this, ButtonIndex]()
				{
					SelectIntroButton(ButtonIndex);
				});
			}
		}
	}

	if (IntroWidget)
	{
		IntroWidget->SetWantsMouse(true);
		IntroWidget->AddToViewport(0);
		ScoreboardOverlay.SetWidget(IntroWidget);
		HideScoreboard();
		SelectIntroButton(0);
		UE_LOG("[MusouGameModeIntro] Intro UI added to viewport");
	}
}

void AMusouGameModeIntro::EndPlay()
{
	ScoreboardOverlay.SetWidget(nullptr);

	if (IntroWidget)
	{
		IntroWidget->RemoveFromParent();
		IntroWidget = nullptr;
	}

	AGameModeBase::EndPlay();
}

void AMusouGameModeIntro::Tick(float DeltaTime)
{
	AGameModeBase::Tick(DeltaTime);

	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	InputSystem& Input = InputSystem::Get();
	if (bScoreboardVisible)
	{
		if (Input.GetKeyDown(VK_ESCAPE))
		{
			HideScoreboard();
		}
		if (Input.GetKeyDown(VK_LEFT))
		{
			ShowPreviousScoreboardPage();
		}
		if (Input.GetKeyDown(VK_RIGHT))
		{
			ShowNextScoreboardPage();
		}
		return;
	}

	if (Input.GetKeyDown(VK_UP))
	{
		MoveIntroSelection(-1);
	}
	if (Input.GetKeyDown(VK_DOWN))
	{
		MoveIntroSelection(1);
	}
	if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
	{
		ExecuteIntroSelection();
	}
}

void AMusouGameModeIntro::SelectIntroButton(int32 ButtonIndex)
{
	if (IntroButtonCount <= 0)
	{
		return;
	}

	SelectedIntroButtonIndex = (ButtonIndex % IntroButtonCount + IntroButtonCount) % IntroButtonCount;
	UpdateIntroSelectionVisuals();
}

void AMusouGameModeIntro::MoveIntroSelection(int32 Delta)
{
	SelectIntroButton(SelectedIntroButtonIndex + Delta);
}

void AMusouGameModeIntro::ExecuteIntroSelection()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	IntroWidget->Click(IntroButtonIds[SelectedIntroButtonIndex]);
}

void AMusouGameModeIntro::UpdateIntroSelectionVisuals()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < IntroButtonCount; ++ButtonIndex)
	{
		IntroWidget->SetClass(IntroButtonIds[ButtonIndex], "selected", ButtonIndex == SelectedIntroButtonIndex);
	}
}

void AMusouGameModeIntro::ShowScoreboard()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	bScoreboardVisible = true;
	ScoreboardOverlay.ShowReadOnly(FMusouScoreboard::LoadEntries(), IntroScoreboardPageSize);
}

void AMusouGameModeIntro::HideScoreboard()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		bScoreboardVisible = false;
		return;
	}

	bScoreboardVisible = false;
	ScoreboardOverlay.Hide();
}

void AMusouGameModeIntro::ShowPreviousScoreboardPage()
{
	ScoreboardOverlay.ShowPreviousPage();
}

void AMusouGameModeIntro::ShowNextScoreboardPage()
{
	ScoreboardOverlay.ShowNextPage();
}
