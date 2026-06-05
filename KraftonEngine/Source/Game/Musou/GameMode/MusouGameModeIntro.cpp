#include "Game/Musou/GameMode/MusouGameModeIntro.h"

#include "Core/Logging/Log.h"
#include "Engine/Runtime/Engine.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>  // PostQuitMessage

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

			IntroWidget->BindClick("exit-button", []()
			{
				UE_LOG("[MusouGameModeIntro] Exit requested from intro UI");
				PostQuitMessage(0);
			});
		}
	}

	if (IntroWidget)
	{
		IntroWidget->SetWantsMouse(true);
		IntroWidget->AddToViewport(0);
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
