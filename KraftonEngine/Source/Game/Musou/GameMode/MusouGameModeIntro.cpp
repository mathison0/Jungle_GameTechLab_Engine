#include "Game/Musou/GameMode/MusouGameModeIntro.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/UI/MusouScoreboardView.h"
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

	FMusouScoreboardViewStyle MakeIntroScoreboardStyle()
	{
		return FMusouScoreboardViewStyle{
			"intro-scoreboard-row",
			"intro-scoreboard-rank",
			"intro-scoreboard-name",
			"intro-scoreboard-score",
			"intro-scoreboard-details",
			"intro-scoreboard-empty",
		};
	}
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

			IntroWidget->BindClick("intro-scoreboard-prev-button", [this]()
			{
				ShowPreviousScoreboardPage();
			});

			IntroWidget->BindClick("intro-scoreboard-next-button", [this]()
			{
				ShowNextScoreboardPage();
			});

			IntroWidget->BindClick("intro-scoreboard-close-button", [this]()
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
		HideScoreboard();
		SelectIntroButton(0);
		UE_LOG("[MusouGameModeIntro] Intro UI added to viewport");
	}
}

void AMusouGameModeIntro::EndPlay()
{
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

	ScoreboardEntries = FMusouScoreboard::LoadEntries();
	ScoreboardPageIndex = 0;
	bScoreboardVisible = true;

	IntroWidget->SetProperty("menu-buttons", "display", "none");
	IntroWidget->SetProperty("intro-scoreboard-panel", "display", "block");
	RenderScoreboardPage();
}

void AMusouGameModeIntro::HideScoreboard()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		bScoreboardVisible = false;
		return;
	}

	bScoreboardVisible = false;
	IntroWidget->SetProperty("intro-scoreboard-panel", "display", "none");
	IntroWidget->SetProperty("menu-buttons", "display", "flex");
}

void AMusouGameModeIntro::ShowPreviousScoreboardPage()
{
	if (ScoreboardPageIndex <= 0)
	{
		return;
	}

	--ScoreboardPageIndex;
	RenderScoreboardPage();
}

void AMusouGameModeIntro::ShowNextScoreboardPage()
{
	const int32 LastPageIndex = FMusouScoreboardView::GetPageCount(ScoreboardEntries.size(), IntroScoreboardPageSize) - 1;
	if (ScoreboardPageIndex >= LastPageIndex)
	{
		return;
	}

	++ScoreboardPageIndex;
	RenderScoreboardPage();
}

void AMusouGameModeIntro::RenderScoreboardPage()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	const int32 PageCount = FMusouScoreboardView::GetPageCount(ScoreboardEntries.size(), IntroScoreboardPageSize);
	ScoreboardPageIndex = FMusouScoreboardView::ClampPageIndex(ScoreboardPageIndex, ScoreboardEntries.size(), IntroScoreboardPageSize);

	IntroWidget->SetText("intro-scoreboard-list", FMusouScoreboardView::MakeRowsRml(ScoreboardEntries, ScoreboardPageIndex, IntroScoreboardPageSize, MakeIntroScoreboardStyle()));
	IntroWidget->SetText("intro-scoreboard-page-label",
		std::to_string(ScoreboardPageIndex + 1) + " / " + std::to_string(PageCount));
	IntroWidget->SetClass("intro-scoreboard-prev-button", "disabled", ScoreboardPageIndex <= 0);
	IntroWidget->SetClass("intro-scoreboard-next-button", "disabled", ScoreboardPageIndex >= PageCount - 1);
}
