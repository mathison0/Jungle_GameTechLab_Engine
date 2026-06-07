#pragma once

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
	void SelectIntroButton(int32 ButtonIndex);
	void MoveIntroSelection(int32 Delta);
	void ExecuteIntroSelection();
	void UpdateIntroSelectionVisuals();
	void ShowScoreboard();
	void HideScoreboard();
	void ShowPreviousScoreboardPage();
	void ShowNextScoreboardPage();

	UUserWidget* IntroWidget = nullptr;
	FMusouScoreboardOverlayPresenter ScoreboardOverlay;
	int32 SelectedIntroButtonIndex = 0;
	bool bScoreboardVisible = false;
};
