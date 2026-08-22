#include "Game/Musou/GameMode/MusouGameModeIntro.h"

#include "Audio/AudioManager.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/Score/MusouScoreboard.h"
#include "Game/Musou/MusouGameSettings.h"
#include "Game/Musou/MusouMatchPersistence.h"
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
	// 세로 메뉴(키보드 내비게이션 대상) — 설정/조작 안내는 우측 상단 코너 버튼(마우스 전용)으로 분리.
	constexpr const char* IntroButtonIds[] = {
		"start-button",
		"tutorial-button",
		"score-button",
		"credits-button",
		"exit-button",
	};

	constexpr int32 IntroButtonCount = 5;
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

	// 메인 메뉴에 도달 = 한 판이 끝난 상태. 다음 게임은 새로 시작되도록 매치 보존 상태를 비운다.
	// (Intro→Play, Intro→Tutorial→Play 등 모든 fresh-start 경로를 한 번에 커버. Play→Play2
	//  트리거만 Intro 를 안 거쳐 보존이 유지됨.)
	FMusouMatchPersistence::Get().Clear();

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
			IntroWidget->BindClick("camera-direction-toggle", [this]()
			{
				FMusouGameSettings::Get().ToggleCameraDirection();
				RefreshCameraDirectionUI();
			});
			IntroWidget->BindClick("camera-shake-toggle", [this]()
			{
				FMusouGameSettings::Get().ToggleCameraShake();
				RefreshCameraDirectionUI();
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

			IntroWidget->BindClick("controls-button", [this]()
			{
				ShowKeyGuide();
			});
			IntroWidget->BindClick("keyguide-close-button", [this]()
			{
				HideKeyGuide();
			});

			IntroWidget->BindClick("tutorial-button", []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Tutorial");
				}
			});

			IntroWidget->BindClick("credits-button", []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Credits");
				}
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
		HideKeyGuide();
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

	if (bKeyGuideVisible)
	{
		if (Input.GetKeyDown(VK_ESCAPE) || Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
		{
			HideKeyGuide();
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
		return !bScoreboardVisible && !bAudioSettingsVisible && !bKeyGuideVisible;
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
	RefreshCameraDirectionUI();
}

void AMusouGameModeIntro::RefreshCameraDirectionUI()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}
	FMusouGameSettings& S = FMusouGameSettings::Get();
	IntroWidget->SetText("camera-direction-label", S.IsCameraDirectionEnabled() ? "카메라 연출: ON" : "카메라 연출: OFF");
	IntroWidget->SetText("camera-shake-label",     S.IsCameraShakeEnabled()     ? "카메라 셰이크: ON" : "카메라 셰이크: OFF");
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

void AMusouGameModeIntro::ShowKeyGuide()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		return;
	}

	bKeyGuideVisible = true;
	IntroMenuNavigator.ClearSelection();
	IntroWidget->SetProperty("keyguide-overlay", "display", "block");
}

void AMusouGameModeIntro::HideKeyGuide()
{
	if (!IntroWidget || !IntroWidget->IsDocumentLoaded())
	{
		bKeyGuideVisible = false;
		return;
	}

	bKeyGuideVisible = false;
	IntroWidget->SetProperty("keyguide-overlay", "display", "none");
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
