#include "Game/Musou/GameMode/GameModeIntro.h"

#include "Core/Logging/Log.h"
#include "Engine/Runtime/Engine.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

AGameModeIntro::AGameModeIntro()
{
}

void AGameModeIntro::StartMatch()
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
		}
	}

	if (IntroWidget)
	{
		IntroWidget->SetWantsMouse(true);
		IntroWidget->AddToViewport(0);
		UE_LOG("[GameModeIntro] Intro UI added to viewport");
	}
}

void AGameModeIntro::EndPlay()
{
	if (IntroWidget)
	{
		IntroWidget->RemoveFromParent();
		IntroWidget = nullptr;
	}

	AGameModeBase::EndPlay();
}
