#pragma once

#include "Game/Musou/Score/MusouScoreboard.h"
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
	void RenderScoreboardPage();

	UUserWidget* IntroWidget = nullptr;
	int32 SelectedIntroButtonIndex = 0;
	bool bScoreboardVisible = false;
	int32 ScoreboardPageIndex = 0;
	TArray<FMusouScoreboardEntry> ScoreboardEntries;
};
