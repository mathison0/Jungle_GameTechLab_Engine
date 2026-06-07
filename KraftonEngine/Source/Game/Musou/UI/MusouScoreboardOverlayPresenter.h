#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/Score/MusouScoreboard.h"

class UUserWidget;

enum class EMusouScoreboardOverlayMode
{
	ReadOnly,
	SaveInput,
};

class FMusouScoreboardOverlayPresenter
{
public:
	void SetWidget(UUserWidget* InWidget);
	void InstallOverlay();

	void ConfigureSaveMode(const TArray<FMusouScoreboardEntry>& Entries, int32 InPageSize, const FString& DefaultPlayerName, const FString& StatusText);
	void ShowReadOnly(const TArray<FMusouScoreboardEntry>& Entries, int32 InPageSize);
	void Show();
	void Hide();
	bool IsVisible() const { return bVisible; }

	void ShowPreviousPage();
	void ShowNextPage();
	void NotifyScoreSubmitted(const TArray<FMusouScoreboardEntry>& Entries, const FString& StatusText);
	void SetSaveStatus(const FString& StatusText);
	void FocusNameInput();

private:
	void SetEntries(const TArray<FMusouScoreboardEntry>& Entries, bool bResetPage);
	void ApplyMode();
	void RenderPage();
	void UpdatePager();
	bool EnsureOverlayInstalled();

	UUserWidget* Widget = nullptr;
	TArray<FMusouScoreboardEntry> ScoreboardEntries;
	EMusouScoreboardOverlayMode Mode = EMusouScoreboardOverlayMode::ReadOnly;
	int32 PageSize = 4;
	int32 PageIndex = 0;
	bool bVisible = false;
	bool bOverlayInstalled = false;
	bool bScoreSubmitted = false;
};
