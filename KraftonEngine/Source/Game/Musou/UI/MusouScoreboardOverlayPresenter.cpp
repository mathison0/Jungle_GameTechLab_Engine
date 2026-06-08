#include "Game/Musou/UI/MusouScoreboardOverlayPresenter.h"

#include "Game/Musou/UI/MusouScoreboardView.h"
#include "UI/UserWidget.h"

#include <algorithm>
#include <string>

namespace
{
	constexpr const char* OverlayHostId = "scoreboard-overlay-host";
	constexpr const char* OverlayId = "scoreboard-overlay";
	constexpr const char* PanelId = "scoreboard-panel";
	constexpr const char* InputRowId = "scoreboard-input-row";
	constexpr const char* NameInputId = "scoreboard-name-input";
	constexpr const char* SaveButtonId = "scoreboard-save-button";
	constexpr const char* SaveStatusId = "scoreboard-save-status";
	constexpr const char* ListId = "scoreboard-list";
	constexpr const char* PageLabelId = "scoreboard-page-label";
	constexpr const char* PrevButtonId = "scoreboard-prev-button";
	constexpr const char* NextButtonId = "scoreboard-next-button";
	constexpr const char* CloseButtonId = "scoreboard-close-button";
	constexpr const char* VictoryMenuId = "victory-menu";

	FMusouScoreboardViewStyle MakeScoreboardStyle()
	{
		return FMusouScoreboardViewStyle{
			"scoreboard-row",
			"scoreboard-rank",
			"scoreboard-name",
			"scoreboard-entry-score",
			"scoreboard-entry-details",
			"scoreboard-empty",
		};
	}

	FString MakeScoreboardOverlayRml()
	{
		// Intro와 InGame이 같은 id/class를 가진 스코어보드 본문을 공유한다.
		return
			"<div id=\"scoreboard-overlay\">"
				"<div id=\"scoreboard-panel\">"
					"<img id=\"scoreboard-panel-bg\" src=\"ScoreBoard_bg.png\" />"
					"<div id=\"scoreboard-title\">스코어 보드</div>"
					"<div id=\"scoreboard-input-row\">"
						"<input id=\"scoreboard-name-input\" type=\"text\" value=\"Player\" />"
						"<div id=\"scoreboard-save-button\">저장</div>"
					"</div>"
					"<div id=\"scoreboard-save-status\"></div>"
					"<div id=\"scoreboard-list\"></div>"
					"<div id=\"scoreboard-pager\">"
						"<div id=\"scoreboard-prev-button\" class=\"scoreboard-page-button\">&lt;</div>"
						"<div id=\"scoreboard-page-label\">1 / 1</div>"
						"<div id=\"scoreboard-next-button\" class=\"scoreboard-page-button\">&gt;</div>"
					"</div>"
					"<div id=\"scoreboard-close-button\" class=\"scoreboard-action-button\">"
						"<img class=\"scoreboard-close-button-frame scoreboard-close-button-frame-normal\" src=\"btn_normal.png\" />"
						"<img class=\"scoreboard-close-button-frame scoreboard-close-button-frame-hover\" src=\"btn_hover.png\" />"
						"<div class=\"scoreboard-close-button-label\">닫기</div>"
					"</div>"
					"<div id=\"victory-menu\">"
						"<div id=\"victory-restart-button\" class=\"pause-button scoreboard-action-button\">"
							"<img class=\"pause-button-frame pause-button-frame-normal\" src=\"btn_normal.png\" />"
							"<img class=\"pause-button-frame pause-button-frame-hover\" src=\"btn_hover.png\" />"
							"<div class=\"pause-button-label\">재도전</div>"
						"</div>"
						"<div id=\"victory-stop-button\" class=\"pause-button scoreboard-action-button\">"
							"<img class=\"pause-button-frame pause-button-frame-normal\" src=\"btn_normal.png\" />"
							"<img class=\"pause-button-frame pause-button-frame-hover\" src=\"btn_hover.png\" />"
							"<div class=\"pause-button-label\">나가기</div>"
						"</div>"
					"</div>"
				"</div>"
			"</div>";
	}
}

void FMusouScoreboardOverlayPresenter::SetWidget(UUserWidget* InWidget)
{
	Widget = InWidget;
	bOverlayInstalled = false;
	if (!Widget)
	{
		bVisible = false;
		ScoreboardEntries.clear();
		return;
	}

	InstallOverlay();
}

void FMusouScoreboardOverlayPresenter::InstallOverlay()
{
	EnsureOverlayInstalled();
}

void FMusouScoreboardOverlayPresenter::ConfigureSaveMode(const TArray<FMusouScoreboardEntry>& Entries, int32 InPageSize, const FString& DefaultPlayerName, const FString& StatusText)
{
	Mode = EMusouScoreboardOverlayMode::SaveInput;
	PageSize = std::max(InPageSize, 1);
	bScoreSubmitted = false;
	SetEntries(Entries, true);

	if (!EnsureOverlayInstalled())
	{
		return;
	}

	Widget->SetValue(NameInputId, DefaultPlayerName);
	Widget->SetText(SaveStatusId, StatusText);
	ApplyMode();
	RenderPage();
}

void FMusouScoreboardOverlayPresenter::ShowReadOnly(const TArray<FMusouScoreboardEntry>& Entries, int32 InPageSize)
{
	Mode = EMusouScoreboardOverlayMode::ReadOnly;
	PageSize = std::max(InPageSize, 1);
	bScoreSubmitted = true;
	SetEntries(Entries, true);
	Show();
}

void FMusouScoreboardOverlayPresenter::Show()
{
	if (!EnsureOverlayInstalled())
	{
		return;
	}

	bVisible = true;
	ApplyMode();
	RenderPage();
	Widget->SetProperty(OverlayHostId, "display", "block");
	Widget->SetProperty(OverlayId, "display", "block");
}

void FMusouScoreboardOverlayPresenter::Hide()
{
	bVisible = false;
	if (!EnsureOverlayInstalled())
	{
		return;
	}

	Widget->SetProperty(OverlayHostId, "display", "none");
	Widget->SetProperty(OverlayId, "display", "none");
}

void FMusouScoreboardOverlayPresenter::ShowPreviousPage()
{
	if (PageIndex <= 0)
	{
		return;
	}

	--PageIndex;
	RenderPage();
}

void FMusouScoreboardOverlayPresenter::ShowNextPage()
{
	const int32 LastPageIndex = FMusouScoreboardView::GetPageCount(ScoreboardEntries.size(), PageSize) - 1;
	if (PageIndex >= LastPageIndex)
	{
		return;
	}

	++PageIndex;
	RenderPage();
}

void FMusouScoreboardOverlayPresenter::NotifyScoreSubmitted(const TArray<FMusouScoreboardEntry>& Entries, const FString& StatusText)
{
	bScoreSubmitted = true;
	SetEntries(Entries, true);
	SetSaveStatus(StatusText);
	ApplyMode();
}

void FMusouScoreboardOverlayPresenter::SetSaveStatus(const FString& StatusText)
{
	if (!EnsureOverlayInstalled())
	{
		return;
	}

	Widget->SetText(SaveStatusId, StatusText);
}

void FMusouScoreboardOverlayPresenter::FocusNameInput()
{
	if (!EnsureOverlayInstalled() || Mode != EMusouScoreboardOverlayMode::SaveInput || bScoreSubmitted)
	{
		return;
	}

	Widget->Focus(NameInputId, true);
}

void FMusouScoreboardOverlayPresenter::SetEntries(const TArray<FMusouScoreboardEntry>& Entries, bool bResetPage)
{
	ScoreboardEntries = Entries;
	if (bResetPage)
	{
		PageIndex = 0;
	}
	else
	{
		PageIndex = FMusouScoreboardView::ClampPageIndex(PageIndex, ScoreboardEntries.size(), PageSize);
	}

	RenderPage();
}

void FMusouScoreboardOverlayPresenter::ApplyMode()
{
	if (!EnsureOverlayInstalled())
	{
		return;
	}

	const bool bSaveMode = Mode == EMusouScoreboardOverlayMode::SaveInput;
	Widget->SetClass(OverlayId, "scoreboard-read-only", !bSaveMode);
	Widget->SetClass(OverlayId, "scoreboard-save-mode", bSaveMode);
	Widget->SetClass(PanelId, "scoreboard-read-only", !bSaveMode);
	Widget->SetClass(PanelId, "scoreboard-save-mode", bSaveMode);
	Widget->SetProperty(InputRowId, "display", bSaveMode ? "flex" : "none");
	Widget->SetProperty(SaveStatusId, "display", bSaveMode ? "block" : "none");
	Widget->SetProperty(SaveButtonId, "display", (bSaveMode && !bScoreSubmitted) ? "block" : "none");
	Widget->SetProperty(CloseButtonId, "display", bSaveMode ? "none" : "block");
	Widget->SetProperty(VictoryMenuId, "display", bSaveMode ? "flex" : "none");
}

void FMusouScoreboardOverlayPresenter::RenderPage()
{
	if (!EnsureOverlayInstalled())
	{
		return;
	}

	PageIndex = FMusouScoreboardView::ClampPageIndex(PageIndex, ScoreboardEntries.size(), PageSize);
	Widget->SetText(ListId, FMusouScoreboardView::MakeRowsRml(ScoreboardEntries, PageIndex, PageSize, MakeScoreboardStyle()));
	UpdatePager();
}

void FMusouScoreboardOverlayPresenter::UpdatePager()
{
	if (!EnsureOverlayInstalled())
	{
		return;
	}

	const int32 PageCount = FMusouScoreboardView::GetPageCount(ScoreboardEntries.size(), PageSize);
	PageIndex = FMusouScoreboardView::ClampPageIndex(PageIndex, ScoreboardEntries.size(), PageSize);

	Widget->SetText(PageLabelId, std::to_string(PageIndex + 1) + " / " + std::to_string(PageCount));
	Widget->SetClass(PrevButtonId, "disabled", PageIndex <= 0);
	Widget->SetClass(NextButtonId, "disabled", PageIndex >= PageCount - 1);
}

bool FMusouScoreboardOverlayPresenter::EnsureOverlayInstalled()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return false;
	}

	if (bOverlayInstalled)
	{
		return true;
	}

	Widget->SetText(OverlayHostId, MakeScoreboardOverlayRml());
	Widget->RegisterEventListeners();
	bOverlayInstalled = true;
	ApplyMode();
	Widget->SetProperty(OverlayHostId, "display", bVisible ? "block" : "none");
	Widget->SetProperty(OverlayId, "display", bVisible ? "block" : "none");
	return true;
}
