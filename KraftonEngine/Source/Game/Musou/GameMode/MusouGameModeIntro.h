#pragma once

#include "Game/Musou/UI/MusouMenuNavigator.h"
#include "Game/Musou/UI/MusouScoreboardOverlayPresenter.h"
#include "GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/Musou/GameMode/MusouGameModeIntro.generated.h"

class UUserWidget;

UCLASS()
class AMusouGameModeIntro : public AGameModeBase
{
public:
	GENERATED_BODY()
	AMusouGameModeIntro();
	~AMusouGameModeIntro() override = default;

	void StartMatch() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;

private:
	void ConfigureIntroMenuNavigator();
	void HandleIntroMenuInput();
	void ShowScoreboard();
	void HideScoreboard();
	void ShowPreviousScoreboardPage();
	void ShowNextScoreboardPage();

	UUserWidget* IntroWidget = nullptr;
	FMusouMenuNavigator IntroMenuNavigator;
	FMusouScoreboardOverlayPresenter ScoreboardOverlay;
	bool bScoreboardVisible = false;
};
