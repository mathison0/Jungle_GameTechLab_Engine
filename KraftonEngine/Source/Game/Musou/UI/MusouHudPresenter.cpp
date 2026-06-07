#include "Game/Musou/UI/MusouHudPresenter.h"

#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/UI/MusouScoreboardView.h"
#include "UI/UserWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
	constexpr float KillPopDuration = 0.18f;
	constexpr float KillMilestoneDuration = 1.25f;
	constexpr float KillMilestonePopDuration = 0.24f;
	constexpr float KillMilestoneShakeDuration = 0.48f;
	constexpr float KillMilestoneBaseMarginLeft = -320.0f;
	constexpr float KillMilestoneBaseMarginTop = -60.0f;

	constexpr float BloodVignetteDuration = 1.8f;
	constexpr float BloodVignetteBaseOpacity = 0.90f;
	constexpr float BloodVignetteMinIntensity = 0.55f;
	constexpr float BloodVignetteFullDamage = 30.0f;
	constexpr float BloodVignetteMinTriggerHealthRatio = 0.15f;
	constexpr float BloodVignetteLowHealthTriggerRatio = 0.35f;

	constexpr float DeathOverlayFadeDuration = 0.85f;
	constexpr float DeathOverlayMaxAlpha = 0.72f;
	constexpr float DeathTitleFadeDelay = 0.25f;
	constexpr float DeathTitleFadeDuration = 1.15f;

	// 승리 결과 연출은 배경 → 타이틀 → 점수/세부 결과 순서로 차례로 드러낸다.
	// 버튼은 점수 영역의 페이드인이 끝난 뒤 활성화된다.
	constexpr float VictoryOverlayFadeDuration = 0.75f;
	constexpr float VictoryOverlayMaxAlpha = 0.66f;
	constexpr float VictoryTitleFadeDelay = 0.15f;
	constexpr float VictoryTitleFadeDuration = 0.85f;
	constexpr float VictoryScoreFadeDelay = 0.55f;
	constexpr float VictoryScoreFadeDuration = 0.75f;
	constexpr int32 ScoreboardPageSize = 4;

	FString MakeScaleTransform(float Scale)
	{
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "scale(%.3f)", Scale);
		return FString(Buffer);
	}

	FString MakePxValue(float Value)
	{
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.1fpx", Value);
		return FString(Buffer);
	}

	FString MakeTextColor(float Alpha, int32 FromRed, int32 FromGreen, int32 FromBlue, int32 ToRed, int32 ToGreen, int32 ToBlue)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const auto LerpChannel = [T](int32 From, int32 To)
		{
			return static_cast<int32>(static_cast<float>(From) + static_cast<float>(To - From) * T + 0.5f);
		};

		const int32 Red = LerpChannel(FromRed, ToRed);
		const int32 Green = LerpChannel(FromGreen, ToGreen);
		const int32 Blue = LerpChannel(FromBlue, ToBlue);

		char Buffer[8] = {};
		std::snprintf(Buffer, sizeof(Buffer), "#%02X%02X%02X", Red, Green, Blue);
		return FString(Buffer);
	}

	FString MakeComboTextColor(float Alpha)
	{
		return MakeTextColor(Alpha, 8, 10, 14, 230, 42, 17);
	}

	float SmoothStep01(float Value)
	{
		const float T = std::clamp(Value, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	FString MakeComboScaleTransform(float Alpha)
	{
		const float Scale = 1.0f + 0.3f * SmoothStep01(Alpha);
		return MakeScaleTransform(Scale);
	}

	FString MakeKillScaleTransform(float Alpha)
	{
		const float Scale = 1.0f + 0.12f * SmoothStep01(Alpha);
		return MakeScaleTransform(Scale);
	}

	FString MakeKillMilestoneTextColor(float Alpha)
	{
		return MakeTextColor(Alpha, 8, 10, 14, 240, 211, 106);
	}

	FString MakeKillMilestoneScaleTransform(float Alpha)
	{
		const float Scale = 1.0f + 0.42f * SmoothStep01(Alpha);
		return MakeScaleTransform(Scale);
	}

	FString MakeOpacityValue(float Alpha)
	{
		const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);

		char Buffer[16] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.3f", ClampedAlpha);
		return FString(Buffer);
	}

	FString MakeOverlayColor(float Alpha)
	{
		const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);
		const int32 AlphaByte = static_cast<int32>(ClampedAlpha * 255.0f + 0.5f);

		char Buffer[16] = {};
		std::snprintf(Buffer, sizeof(Buffer), "#080A0E%02X", AlphaByte);
		return FString(Buffer);
	}

	FString MakeVictoryOverlayColor(float Alpha)
	{
		const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);
		const int32 AlphaByte = static_cast<int32>(ClampedAlpha * 255.0f + 0.5f);

		char Buffer[16] = {};
		std::snprintf(Buffer, sizeof(Buffer), "#07110C%02X", AlphaByte);
		return FString(Buffer);
	}

	// 결과 화면의 보조 정보 문구. 실제 스코어보드 UI가 들어오면 같은 Result를 넘겨 재사용한다.
	FString MakeVictoryDetailsText(const FMusouMatchResult& Result)
	{
		char Buffer[256] = {};
		std::snprintf(Buffer, sizeof(Buffer),
			"<span class=\"victory-detail-item\">K.O. %d</span>"
			"<span class=\"victory-detail-separator\">/</span>"
			"<span class=\"victory-detail-item\">Max Combo %d</span>"
			"<span class=\"victory-detail-separator\">/</span>"
			"<span class=\"victory-detail-item\">Time %.1fs</span>",
			Result.KillCount,
			Result.MaxCombo,
			Result.MatchTime);
		return FString(Buffer);
	}

	FMusouScoreboardViewStyle MakeVictoryScoreboardStyle()
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
}

void FMusouHudPresenter::SetWidget(UUserWidget* InWidget)
{
	Widget = InWidget;
}

void FMusouHudPresenter::Tick(float DeltaTime, const AMusouGameState* MusouState, float PlayerHealthRatio)
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	if (MusouState)
	{
		UpdateStatusHud(MusouState, PlayerHealthRatio);
		UpdateKillHud(DeltaTime, MusouState);
		UpdateComboHud(MusouState);
	}

	UpdateBloodVignette(DeltaTime);
	UpdateDeathOverlay(DeltaTime);
	UpdateVictoryOverlay(DeltaTime);
}

void FMusouHudPresenter::SetPauseMenuVisible(bool bVisible)
{
	// 결과 오버레이가 떠 있을 때 pause 메뉴가 겹치면 마우스 포커스가 꼬이므로 무시한다.
	if (IsResultOverlayVisible() || !Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	if (bVisible)
	{
		Widget->SetProperty("pause-overlay", "background-color", "#1616165c");
		Widget->SetProperty("death-title", "display", "none");
		Widget->SetProperty("death-title", "opacity", "0");
		Widget->SetProperty("pause-menu", "display", "block");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetProperty("resume-button", "display", "block");
		Widget->SetProperty("restart-button", "display", "block");
		Widget->SetProperty("stop-button", "display", "block");
	}

	Widget->SetProperty("pause-overlay", "display", bVisible ? "block" : "none");
	Widget->SetWantsMouse(bVisible);
}

void FMusouHudPresenter::NotifyPlayerDamaged(float Damage, float PlayerCurrentHealth, float PlayerMaxHealth)
{
	if (Damage <= 0.0f || PlayerMaxHealth <= 0.0f)
	{
		return;
	}

	const bool bLowHealth = PlayerCurrentHealth <= PlayerMaxHealth * BloodVignetteLowHealthTriggerRatio;
	if (!bLowHealth && Damage < PlayerMaxHealth * BloodVignetteMinTriggerHealthRatio)
	{
		return;
	}

	const float DamageIntensity = std::clamp(Damage / BloodVignetteFullDamage, BloodVignetteMinIntensity, 1.0f);
	BloodVignetteIntensity = std::max(BloodVignetteIntensity, DamageIntensity);
	BloodVignetteRemaining = BloodVignetteDuration;
}

void FMusouHudPresenter::StartDeathOverlay()
{
	if (bDeathOverlayVisible || bVictoryOverlayVisible)
	{
		return;
	}

	bDeathOverlayVisible = true;
	bDeathButtonsVisible = false;
	DeathOverlayElapsed = 0.0f;

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-overlay", "background-color", MakeOverlayColor(0.0f));
	Widget->SetProperty("death-title", "display", "block");
	Widget->SetProperty("death-title", "opacity", "0");
	Widget->SetProperty("victory-title", "display", "none");
	Widget->SetProperty("victory-score", "display", "none");
	Widget->SetProperty("victory-details", "display", "none");
	Widget->SetProperty("scoreboard-panel", "display", "none");
	Widget->SetProperty("pause-menu", "display", "none");
	Widget->SetProperty("death-menu", "display", "none");
	Widget->SetProperty("victory-menu", "display", "none");
	Widget->SetWantsMouse(false);
}

void FMusouHudPresenter::StartVictoryOverlay(const FMusouMatchResult& Result, const TArray<FMusouScoreboardEntry>& ScoreboardEntries)
{
	if (bVictoryOverlayVisible || bDeathOverlayVisible)
	{
		return;
	}

	bVictoryOverlayVisible = true;
	bVictoryButtonsVisible = false;
	bVictoryScoreboardVisible = false;
	bVictoryScoreSubmitted = false;
	ScoreboardPageIndex = 0;
	VictoryOverlayElapsed = 0.0f;
	VictoryHealthRatio = std::clamp(Result.PlayerHealthRatio, 0.0f, 1.0f);

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetProperty("blood-vignette", "visibility", "hidden");
	Widget->SetProperty("blood-vignette", "opacity", "0");
	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-overlay", "background-color", MakeVictoryOverlayColor(0.0f));

	// 같은 pause-overlay 레이어를 공유하므로 다른 메뉴/타이틀을 먼저 모두 접는다.
	Widget->SetProperty("death-title", "display", "none");
	Widget->SetProperty("death-menu", "display", "none");
	Widget->SetProperty("pause-menu", "display", "none");
	Widget->SetProperty("victory-title", "display", "block");
	Widget->SetProperty("victory-title", "opacity", "0");
	Widget->SetProperty("victory-score", "display", "block");
	Widget->SetProperty("victory-score", "opacity", "0");
	Widget->SetProperty("victory-details", "display", "block");
	Widget->SetProperty("victory-details", "opacity", "0");
	Widget->SetProperty("scoreboard-panel", "display", "none");
	Widget->SetProperty("scoreboard-save-button", "display", "block");
	Widget->SetProperty("victory-menu", "display", "none");

	// 표시 값은 승리 확정 시점에 고정된 Result만 사용한다.
	Widget->SetText("victory-score", FString("score: ") + std::to_string(static_cast<long long>(Result.Score)));
	Widget->SetText("victory-details", MakeVictoryDetailsText(Result));
	Widget->SetText("scoreboard-save-status", "이름 입력 후 저장하면 랭킹에 반영됩니다.");
	Widget->SetValue("scoreboard-name-input", "Player");
	SetScoreboardEntries(ScoreboardEntries, true);
	Widget->SetWantsMouse(false);
}

void FMusouHudPresenter::NotifyVictoryScoreSubmitted(const TArray<FMusouScoreboardEntry>& ScoreboardEntries)
{
	bVictoryScoreSubmitted = true;
	SetScoreboardEntries(ScoreboardEntries, true);

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetText("scoreboard-save-status", "저장 완료");
	Widget->SetProperty("scoreboard-save-button", "display", "none");

	if (bVictoryScoreboardVisible)
	{
		bVictoryButtonsVisible = true;
		Widget->SetProperty("victory-menu", "display", "flex");
		Widget->SetWantsMouse(true);
	}
}

void FMusouHudPresenter::NotifyVictoryScoreSaveFailed()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetText("scoreboard-save-status", "저장 실패");
}

void FMusouHudPresenter::ShowPreviousScoreboardPage()
{
	if (ScoreboardPageIndex <= 0)
	{
		return;
	}

	--ScoreboardPageIndex;
	RenderScoreboardPage();
}

void FMusouHudPresenter::ShowNextScoreboardPage()
{
	const int32 LastPageIndex = FMusouScoreboardView::GetPageCount(ScoreboardEntries.size(), ScoreboardPageSize) - 1;
	if (ScoreboardPageIndex >= LastPageIndex)
	{
		return;
	}

	++ScoreboardPageIndex;
	RenderScoreboardPage();
}

void FMusouHudPresenter::UpdateStatusHud(const AMusouGameState* MusouState, float PlayerHealthRatio)
{
	float DisplayHealthRatio = PlayerHealthRatio;
	if (bDeathOverlayVisible)
	{
		DisplayHealthRatio = 0.0f;
	}
	else if (bVictoryOverlayVisible)
	{
		// 승리 후에는 EndMatch가 Pawn을 UnPossess하므로 GameMode의 fallback HP(1.0f)를 쓰지 않는다.
		DisplayHealthRatio = VictoryHealthRatio;
	}

	Widget->SetAttribute("hp-bar", "value", std::clamp(DisplayHealthRatio, 0.0f, 1.0f));
	Widget->SetText("score-counter", FString("score: ") + std::to_string(static_cast<long long>(MusouState->GetScore())));
	Widget->SetAttribute("musou-bar", "value", MusouState->GetMusouGauge());
	Widget->SetProperty("musou-ready-label", "visibility", MusouState->IsMusouGaugeFull() ? "visible" : "hidden");
}

void FMusouHudPresenter::UpdateKillHud(float DeltaTime, const AMusouGameState* MusouState)
{
	const int32 KillCount = MusouState->GetKillCount();

	if (!bKillHudInitialized)
	{
		LastHudKillCount = KillCount;
		bKillHudInitialized = true;
	}
	else if (KillCount > LastHudKillCount)
	{
		const int32 LastMilestone = LastHudKillCount / 10;
		const int32 CurrentMilestone = KillCount / 10;
		LastHudKillCount = KillCount;
		KillPopRemaining = KillPopDuration;

		const int32 ReachedMilestone = CurrentMilestone * 10;
		if (CurrentMilestone > LastMilestone && ReachedMilestone > LastDisplayedKillMilestone)
		{
			ActiveKillMilestone = ReachedMilestone;
			LastDisplayedKillMilestone = ActiveKillMilestone;
			KillMilestoneElapsed = 0.0f;
			KillMilestoneRemaining = KillMilestoneDuration;
		}
	}
	else if (KillCount != LastHudKillCount)
	{
		LastHudKillCount = KillCount;
	}

	const float KillPopAlpha = KillPopDuration > 0.0f
		? std::clamp(KillPopRemaining / KillPopDuration, 0.0f, 1.0f)
		: 0.0f;

	Widget->SetText("kill-count-value", std::to_string(KillCount));
	Widget->SetProperty("kill-count-value", "transform", MakeKillScaleTransform(KillPopAlpha));
	KillPopRemaining = std::max(0.0f, KillPopRemaining - DeltaTime);

	if (KillMilestoneRemaining > 0.0f && ActiveKillMilestone > 0)
	{
		const float FadeAlpha = std::clamp(KillMilestoneRemaining / KillMilestoneDuration, 0.0f, 1.0f);
		const float PopAlpha = std::clamp(1.0f - (KillMilestoneElapsed / KillMilestonePopDuration), 0.0f, 1.0f);
		const float ShakeAlpha = std::clamp(1.0f - (KillMilestoneElapsed / KillMilestoneShakeDuration), 0.0f, 1.0f) * FadeAlpha;
		const float ShakeX = static_cast<float>(std::sin(KillMilestoneElapsed * 78.0f)) * 6.0f * ShakeAlpha;
		const float ShakeY = static_cast<float>(std::sin(KillMilestoneElapsed * 113.0f + 0.7f)) * 2.5f * ShakeAlpha;

		Widget->SetProperty("kill-milestone", "display", "block");
		Widget->SetText("kill-milestone", std::to_string(ActiveKillMilestone) + " K.O.");
		Widget->SetProperty("kill-milestone", "color", MakeKillMilestoneTextColor(FadeAlpha));
		Widget->SetProperty("kill-milestone", "transform", MakeKillMilestoneScaleTransform(PopAlpha));
		Widget->SetProperty("kill-milestone", "margin-left", MakePxValue(KillMilestoneBaseMarginLeft + ShakeX));
		Widget->SetProperty("kill-milestone", "margin-top", MakePxValue(KillMilestoneBaseMarginTop + ShakeY));

		KillMilestoneElapsed += DeltaTime;
		KillMilestoneRemaining = std::max(0.0f, KillMilestoneRemaining - DeltaTime);
	}
	else
	{
		Widget->SetProperty("kill-milestone", "display", "none");
	}
}

void FMusouHudPresenter::UpdateComboHud(const AMusouGameState* MusouState)
{
	const int32 Combo = MusouState->GetCombo();
	const float ComboWindow = MusouState->ComboWindow;
	const float ComboRemaining = MusouState->GetComboRemaining();
	const float DisplayAlpha = (Combo > 0 && ComboWindow > 0.0f)
		? std::clamp(ComboRemaining / ComboWindow, 0.0f, 1.0f)
		: 0.0f;

	if (Combo > 0 && DisplayAlpha > 0.03f)
	{
		Widget->SetProperty("combo-counter-frame", "display", "block");
		Widget->SetText("combo-counter", FString("Combo ") + std::to_string(Combo));
	}
	else
	{
		Widget->SetProperty("combo-counter-frame", "display", "none");
		Widget->SetText("combo-counter", "");
	}

	Widget->SetProperty("combo-counter", "opacity", "1.0");
	Widget->SetProperty("combo-counter", "color", MakeComboTextColor(DisplayAlpha));
	Widget->SetProperty("combo-counter", "transform", MakeComboScaleTransform(DisplayAlpha));
}

void FMusouHudPresenter::UpdateBloodVignette(float DeltaTime)
{
	const float BloodTimeAlpha = BloodVignetteDuration > 0.0f
		? std::clamp(BloodVignetteRemaining / BloodVignetteDuration, 0.0f, 1.0f)
		: 0.0f;
	const float BloodFadeAlpha = BloodTimeAlpha * BloodTimeAlpha;
	const float BloodAlpha = BloodVignetteBaseOpacity * BloodVignetteIntensity * BloodFadeAlpha;

	if (BloodAlpha > 0.01f)
	{
		Widget->SetProperty("blood-vignette", "opacity", MakeOpacityValue(BloodAlpha));
		Widget->SetProperty("blood-vignette", "visibility", "visible");
	}
	else
	{
		Widget->SetProperty("blood-vignette", "opacity", "0");
		Widget->SetProperty("blood-vignette", "visibility", "hidden");
	}

	BloodVignetteRemaining = std::max(0.0f, BloodVignetteRemaining - DeltaTime);
	if (BloodVignetteRemaining <= 0.0f)
	{
		BloodVignetteIntensity = 0.0f;
	}
}

void FMusouHudPresenter::UpdateDeathOverlay(float DeltaTime)
{
	if (!bDeathOverlayVisible)
	{
		return;
	}

	DeathOverlayElapsed += DeltaTime;

	const float OverlayAlpha = DeathOverlayMaxAlpha * SmoothStep01(DeathOverlayElapsed / DeathOverlayFadeDuration);
	const float TitleAlpha = SmoothStep01((DeathOverlayElapsed - DeathTitleFadeDelay) / DeathTitleFadeDuration);

	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-overlay", "background-color", MakeOverlayColor(OverlayAlpha));
	Widget->SetProperty("death-title", "display", "block");
	Widget->SetProperty("death-title", "opacity", MakeOpacityValue(TitleAlpha));

	if (TitleAlpha < 1.0f)
	{
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetWantsMouse(false);
		return;
	}

	if (!bDeathButtonsVisible)
	{
		bDeathButtonsVisible = true;
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "block");
		Widget->SetWantsMouse(true);
	}
}

void FMusouHudPresenter::UpdateVictoryOverlay(float DeltaTime)
{
	if (!bVictoryOverlayVisible)
	{
		return;
	}

	VictoryOverlayElapsed += DeltaTime;

	const float OverlayAlpha = VictoryOverlayMaxAlpha * SmoothStep01(VictoryOverlayElapsed / VictoryOverlayFadeDuration);
	const float TitleAlpha = SmoothStep01((VictoryOverlayElapsed - VictoryTitleFadeDelay) / VictoryTitleFadeDuration);
	const float ScoreAlpha = SmoothStep01((VictoryOverlayElapsed - VictoryScoreFadeDelay) / VictoryScoreFadeDuration);

	// overlay는 매 프레임 직접 값을 갱신해 RML transition 의존 없이 동일한 타이밍을 보장한다.
	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-overlay", "background-color", MakeVictoryOverlayColor(OverlayAlpha));
	Widget->SetProperty("victory-title", "display", "block");
	Widget->SetProperty("victory-title", "opacity", MakeOpacityValue(TitleAlpha));
	Widget->SetProperty("victory-score", "display", "block");
	Widget->SetProperty("victory-score", "opacity", MakeOpacityValue(ScoreAlpha));
	Widget->SetProperty("victory-details", "display", "block");
	Widget->SetProperty("victory-details", "opacity", MakeOpacityValue(ScoreAlpha));

	if (ScoreAlpha < 1.0f)
	{
		// 결과 문구가 완전히 나타나기 전까지는 실수 클릭/엔터 입력을 막는다.
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetProperty("victory-menu", "display", "none");
		Widget->SetProperty("scoreboard-panel", "display", "none");
		Widget->SetWantsMouse(false);
		return;
	}

	if (!bVictoryScoreboardVisible)
	{
		bVictoryScoreboardVisible = true;
		bVictoryButtonsVisible = true;
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetProperty("scoreboard-panel", "display", "block");
		Widget->SetProperty("victory-menu", "display", "flex");
		Widget->SetWantsMouse(true);

		// 스코어보드가 열린 직후 바로 이름을 입력할 수 있도록 입력창에 포커스를 준다.
		Widget->Focus("scoreboard-name-input", true);
	}
}

void FMusouHudPresenter::SetScoreboardEntries(const TArray<FMusouScoreboardEntry>& Entries, bool bResetPage)
{
	ScoreboardEntries = Entries;
	if (bResetPage)
	{
		ScoreboardPageIndex = 0;
	}
	else
	{
		ScoreboardPageIndex = FMusouScoreboardView::ClampPageIndex(ScoreboardPageIndex, ScoreboardEntries.size(), ScoreboardPageSize);
	}

	RenderScoreboardPage();
}

void FMusouHudPresenter::RenderScoreboardPage()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	ScoreboardPageIndex = FMusouScoreboardView::ClampPageIndex(ScoreboardPageIndex, ScoreboardEntries.size(), ScoreboardPageSize);
	Widget->SetText("scoreboard-list", FMusouScoreboardView::MakeRowsRml(ScoreboardEntries, ScoreboardPageIndex, ScoreboardPageSize, MakeVictoryScoreboardStyle()));
	UpdateScoreboardPager();
}

void FMusouHudPresenter::UpdateScoreboardPager()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	const int32 PageCount = FMusouScoreboardView::GetPageCount(ScoreboardEntries.size(), ScoreboardPageSize);
	ScoreboardPageIndex = FMusouScoreboardView::ClampPageIndex(ScoreboardPageIndex, ScoreboardEntries.size(), ScoreboardPageSize);

	Widget->SetText("scoreboard-page-label",
		std::to_string(ScoreboardPageIndex + 1) + " / " + std::to_string(PageCount));
	Widget->SetClass("scoreboard-prev-button", "disabled", ScoreboardPageIndex <= 0);
	Widget->SetClass("scoreboard-next-button", "disabled", ScoreboardPageIndex >= PageCount - 1);
}
