#include "Game/Musou/GameMode/MusouGameModeIntro.h"

#include "Audio/AudioManager.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/Score/MusouScoreboard.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

#include <algorithm>
#include <cstdio>

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
	constexpr float BGMVolumeStep = 0.1f;

	FString MakeBGMVolumeText(float Volume)
	{
		const int32 Percent = static_cast<int32>(std::clamp(Volume, 0.0f, 1.0f) * 100.0f + 0.5f);
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "BGM %d%%", Percent);
		return FString(Buffer);
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
			IntroWidget->BindClick("settings-button", [this]()
			{
				ShowAudioSettings();
			});
			IntroWidget->BindClick("bgm-volume-down-button", [this]()
			{
				AdjustBGMVolume(-BGMVolumeStep);
			});
			IntroWidget->BindClick("bgm-volume-up-button", [this]()
			{
				AdjustBGMVolume(BGMVolumeStep);
			});
			IntroWidget->BindClick("audio-settings-close-button", [this]()
			{
				HideAudioSettings();
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

			ConfigureIntroMenuNavigator();
		}
	}

	if (IntroWidget)
	{
		IntroWidget->SetWantsMouse(true);
		IntroWidget->AddToViewport(0);
		ScoreboardOverlay.SetWidget(IntroWidget);
		HideScoreboard();
		HideAudioSettings();
		IntroMenuNavigator.Select(0);
		UE_LOG("[MusouGameModeIntro] Intro UI added to viewport");
	}
}

void AMusouGameModeIntro::EndPlay()
{
	ScoreboardOverlay.SetWidget(nullptr);
	IntroMenuNavigator.SetWidget(nullptr);

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
	if (bAudioSettingsVisible)
	{
		if (Input.GetKeyDown(VK_ESCAPE))
		{
			HideAudioSettings();
		}
		else
		{
			HandleAudioSettingsInput();
		}
		return;
	}

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

	HandleIntroMenuInput();
}

void AMusouGameModeIntro::ConfigureIntroMenuNavigator()
{
	if (!IntroWidget)
	{
		return;
	}

	IntroMenuNavigator.SetWidget(IntroWidget);
	IntroMenuNavigator.SetButtons(IntroButtonIds, IntroButtonCount);
	IntroMenuNavigator.BindHoverHandlers([this]()
	{
		return !bScoreboardVisible && !bAudioSettingsVisible;
	});
}

void AMusouGameModeIntro::HandleIntroMenuInput()
{
	IntroMenuNavigator.HandleVerticalInput(InputSystem::Get());
}

void AMusouGameModeIntro::ShowAudioSettings()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	bAudioSettingsVisible = true;
	IntroMenuNavigator.ClearSelection();
	IntroWidget->SetProperty("audio-settings-overlay", "display", "block");
	RefreshAudioSettingsUI();
}

void AMusouGameModeIntro::HideAudioSettings()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		bAudioSettingsVisible = false;
		return;
	}

	bAudioSettingsVisible = false;
	IntroWidget->SetProperty("audio-settings-overlay", "display", "none");
	IntroMenuNavigator.EnsureSelection(1);
}

void AMusouGameModeIntro::AdjustBGMVolume(float Delta)
{
	FAudioManager& AudioManager = FAudioManager::Get();
	AudioManager.SetBGMVolume(AudioManager.GetBGMVolume() + Delta);
	RefreshAudioSettingsUI();
}

void AMusouGameModeIntro::RefreshAudioSettingsUI()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	const float Volume = std::clamp(FAudioManager::Get().GetBGMVolume(), 0.0f, 1.0f);
	IntroWidget->SetText("bgm-volume-label", MakeBGMVolumeText(Volume));
	IntroWidget->SetAttribute("bgm-volume-bar", "value", Volume);
}

void AMusouGameModeIntro::HandleAudioSettingsInput()
{
	InputSystem& Input = InputSystem::Get();
	if (Input.GetKeyDown(VK_LEFT))
	{
		AdjustBGMVolume(-BGMVolumeStep);
	}
	if (Input.GetKeyDown(VK_RIGHT))
	{
		AdjustBGMVolume(BGMVolumeStep);
	}
	if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
	{
		HideAudioSettings();
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
	IntroMenuNavigator.EnsureSelection();
}

void AMusouGameModeIntro::ShowPreviousScoreboardPage()
{
	ScoreboardOverlay.ShowPreviousPage();
}

void AMusouGameModeIntro::ShowNextScoreboardPage()
{
	ScoreboardOverlay.ShowNextPage();
}
