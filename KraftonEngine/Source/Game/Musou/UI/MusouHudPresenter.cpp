#include "Game/Musou/UI/MusouHudPresenter.h"

#include "Game/Musou/GameMode/MusouGameState.h"
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
	constexpr float DeathTitleFadeDelay = 0.25f;
	constexpr float DeathTitleFadeDuration = 1.15f;

	constexpr float StoryDialogCharsPerSecond = 24.0f;
	constexpr float StoryDialogIndicatorPulseSpeed = 5.0f;
	constexpr float StoryDialogFadeOutDuration = 1.0f;
	constexpr float StoryDialogCutsceneFadeDuration = 1.5f;
	constexpr const char* StoryDialogBlackOverlayColor = "#000000ff";
	constexpr const char* StoryDialogTransparentOverlayColor = "#00000000";
	constexpr int32 StoryDialogCutsceneCount = 3;
	constexpr const char* CombatHudElementIds[] = {
		"status-panel",
		"score-counter",
		"right-hud",
		"kill-counter",
		"kill-milestone",
		"blood-vignette",
		"boss-health-panel",
	};

	const char* StoryDialogCutsceneElementIds[StoryDialogCutsceneCount] = {
		"story-dialog-intro-cutscene-1",
		"story-dialog-intro-cutscene-2",
		"story-dialog-intro-cutscene-3",
	};

	// 승리 결과 연출은 배경 → 타이틀 → 점수/세부 결과 순서로 차례로 드러낸다.
	// 버튼은 점수 영역의 페이드인이 끝난 뒤 활성화된다.
	constexpr float VictoryOverlayFadeDuration = 0.75f;
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

	const TArray<FString>& GetIntroDialogPages()
	{
		static const TArray<FString> Pages = {
			"고대의 파괴자, 골렘.",
			"오래전 봉인된 그 존재를 부활시키기 위해\n어둠 속에 있던 악의 세력이 다시 움직이기 시작했다.",
			"골렘이 깨어나는 순간,\n세계의 평화는 무너지고 모든 생명은 파멸을 맞이할 것이다.",
			"우리는 그 부활을 막기 위해 이 전장에 섰다.",
			"적의 군세를 돌파하고,\n골렘의 부활을 막아라.",
		};
		return Pages;
	}

	const TArray<FString>& GetOutroDialogPages()
	{
		static const TArray<FString> Pages = {
			"골렘의 거대한 몸이 무너져 내렸다.",
			"대지를 뒤흔들던 포효는 사라지고,\n전장을 뒤덮었던 어둠도 천천히 걷히기 시작했다.",
			"수많은 싸움과 희생 끝에,\n세계는 다시 평화를 되찾았다.",
			"하지만 이 평화가 영원할지는 아무도 알 수 없다.",
			"언젠가 또 다른 어둠이 찾아온다면,\n우리는 다시 검을 들 것이다.",
			"정글의 땅을 지키기 위해.",
		};
		return Pages;
	}

	const TArray<FString>& GetFinalBossDialogPages()
	{
		static const TArray<FString> Pages = {
			"……늦었다.",
			"오래전 세계를 멸망 직전까지 몰아넣었던 그 존재가\n다시 이 땅에 모습을 드러냈다.",
			"깨어난 골렘을 쓰러뜨리지 못한다면,\n세계의 평화는 이곳에서 사라질 것이다.",
			"검을 들어라.",
			"이것이 마지막 전투다.",
		};
		return Pages;
	}

	int32 GetIntroDialogCutsceneIndex(int32 PageIndex)
	{
		switch (PageIndex)
		{
		case 0:
			return 0;
		case 1:
		case 2:
			return 1;
		case 3:
			return 2;
		default:
			return -1;
		}
	}

	int32 GetUtf8CodepointLength(unsigned char LeadByte)
	{
		if ((LeadByte & 0x80) == 0)
		{
			return 1;
		}
		if ((LeadByte & 0xE0) == 0xC0)
		{
			return 2;
		}
		if ((LeadByte & 0xF0) == 0xE0)
		{
			return 3;
		}
		if ((LeadByte & 0xF8) == 0xF0)
		{
			return 4;
		}
		return 1;
	}

	FString EscapeRmlText(const FString& Text)
	{
		FString Escaped;
		Escaped.reserve(Text.size());
		for (char C : Text)
		{
			switch (C)
			{
			case '&': Escaped += "&amp;"; break;
			case '<': Escaped += "&lt;"; break;
			case '>': Escaped += "&gt;"; break;
			case '"': Escaped += "&quot;"; break;
			case '\'': Escaped += "&#39;"; break;
			default: Escaped.push_back(C); break;
			}
		}
		return Escaped;
	}

	int32 CountStoryDialogGlyphs(const FString& Text)
	{
		int32 Count = 0;
		for (size_t Index = 0; Index < Text.size();)
		{
			const char C = Text[Index];
			if (C == '\r' || C == '\n')
			{
				++Index;
				continue;
			}

			const int32 Length = std::min<int32>(
				GetUtf8CodepointLength(static_cast<unsigned char>(C)),
				static_cast<int32>(Text.size() - Index));
			Index += static_cast<size_t>(Length);
			++Count;
		}
		return Count;
	}

	FString MakeStoryDialogTextRml(const FString& Text, float VisibleGlyphs)
	{
		const int32 FullyVisibleGlyphs = static_cast<int32>(std::floor(std::max(0.0f, VisibleGlyphs)));
		const float CurrentGlyphAlpha = std::clamp(VisibleGlyphs - static_cast<float>(FullyVisibleGlyphs), 0.0f, 1.0f);

		FString Rml;
		Rml.reserve(Text.size() * 8);

		int32 GlyphIndex = 0;
		for (size_t Index = 0; Index < Text.size();)
		{
			const char C = Text[Index];
			if (C == '\r')
			{
				++Index;
				continue;
			}
			if (C == '\n')
			{
				Rml += "<br/>";
				++Index;
				continue;
			}

			const int32 Length = std::min<int32>(
				GetUtf8CodepointLength(static_cast<unsigned char>(C)),
				static_cast<int32>(Text.size() - Index));
			const FString Glyph = Text.substr(Index, static_cast<size_t>(Length));

			float GlyphAlpha = 0.0f;
			if (GlyphIndex < FullyVisibleGlyphs)
			{
				GlyphAlpha = 1.0f;
			}
			else if (GlyphIndex == FullyVisibleGlyphs)
			{
				GlyphAlpha = CurrentGlyphAlpha;
			}

			Rml += "<span class=\"story-dialog-glyph\" style=\"opacity:";
			Rml += MakeOpacityValue(GlyphAlpha);
			Rml += ";\">";
			Rml += (C == ' ') ? "&#160;" : EscapeRmlText(Glyph);
			Rml += "</span>";

			Index += static_cast<size_t>(Length);
			++GlyphIndex;
		}

		return Rml;
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

}

void FMusouHudPresenter::SetWidget(UUserWidget* InWidget)
{
	Widget = InWidget;
	ScoreboardOverlay.SetWidget(InWidget);
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
	UpdateStoryDialog(DeltaTime);
	UpdateDeathOverlay(DeltaTime);
	UpdateVictoryOverlay(DeltaTime);
}

void FMusouHudPresenter::SetHudVisible(bool bVisible)
{
	bCombatHudVisible = bVisible;

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetProperty("hud-root", "visibility", "visible");
	for (const char* ElementId : CombatHudElementIds)
	{
		Widget->SetProperty(ElementId, "visibility", bVisible ? "visible" : "hidden");
	}
}

bool FMusouHudPresenter::StartIntroDialog()
{
	return StartStoryDialog(EStoryDialogKind::Intro);
}

bool FMusouHudPresenter::StartOutroDialog()
{
	return StartStoryDialog(EStoryDialogKind::Outro);
}

bool FMusouHudPresenter::StartFinalBossDialog()
{
	return StartStoryDialog(EStoryDialogKind::FinalBoss);
}

bool FMusouHudPresenter::StartBossSequenceDialog(const FString& Text)
{
	if (Text.empty()
		|| bStoryDialogFadeOutActive
		|| (StoryDialogKind != EStoryDialogKind::None && StoryDialogKind != EStoryDialogKind::BossSequence))
	{
		return false;
	}

	BossSequenceDialogPages.clear();
	BossSequenceDialogPages.push_back(Text);
	if (!StartStoryDialog(EStoryDialogKind::BossSequence))
	{
		BossSequenceDialogPages.clear();
		return false;
	}

	return true;
}

void FMusouHudPresenter::FinishBossSequenceDialog()
{
	if (StoryDialogKind == EStoryDialogKind::BossSequence)
	{
		FinishStoryDialog();
		return;
	}

	BossSequenceDialogPages.clear();
}

bool FMusouHudPresenter::AdvanceStoryDialog()
{
	if (StoryDialogKind == EStoryDialogKind::None || bStoryDialogFadeOutActive)
	{
		return false;
	}

	const TArray<FString>& Pages = GetActiveStoryDialogPages();
	if (StoryDialogPageIndex < 0 || StoryDialogPageIndex >= static_cast<int32>(Pages.size()))
	{
		FinishStoryDialog();
		return true;
	}

	const int32 CurrentGlyphCount = CountStoryDialogGlyphs(Pages[StoryDialogPageIndex]);
	if (StoryDialogTextProgress < static_cast<float>(CurrentGlyphCount))
	{
		StoryDialogTextProgress = static_cast<float>(CurrentGlyphCount);
		RenderStoryDialogText();
		return false;
	}

	if (StoryDialogPageIndex + 1 < static_cast<int32>(Pages.size()))
	{
		++StoryDialogPageIndex;
		StoryDialogTextProgress = 0.0f;
		StoryDialogElapsed = 0.0f;
		Widget->SetProperty("story-dialog-next-indicator", "visibility", "hidden");
		Widget->SetProperty("story-dialog-next-indicator", "opacity", "0");
		BeginStoryDialogCutsceneTransition();
		RenderStoryDialogText();
		return false;
	}

	FinishStoryDialog();
	return true;
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
		Widget->SetProperty("death-title", "display", "none");
		Widget->SetProperty("death-title", "opacity", "0");
		Widget->SetProperty("pause-menu-overlay", "display", "block");
		Widget->SetProperty("death-overlay", "display", "none");
		Widget->SetProperty("death-overlay", "opacity", "0");
		Widget->SetProperty("victory-overlay", "display", "none");
		Widget->SetProperty("victory-overlay", "opacity", "0");
		Widget->SetProperty("pause-menu", "display", "block");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetProperty("resume-button", "display", "block");
		Widget->SetProperty("restart-button", "display", "block");
		Widget->SetProperty("stop-button", "display", "block");
	}
	else
	{
		Widget->SetProperty("pause-menu-overlay", "display", "none");
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

void FMusouHudPresenter::ShowBossHealth(const FString& BossName, float HealthRatio)
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetProperty("boss-health-panel", "display", "block");
	Widget->SetText("boss-name", BossName.empty() ? FString("BOSS") : BossName);
	Widget->SetAttribute("boss-hp-bar", "value", std::clamp(HealthRatio, 0.0f, 1.0f));
}

void FMusouHudPresenter::HideBossHealth()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetProperty("boss-health-panel", "display", "none");
	Widget->SetText("boss-name", "");
	Widget->SetAttribute("boss-hp-bar", "value", 0.0f);
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

	HideBossHealth();
	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-menu-overlay", "display", "none");
	Widget->SetProperty("death-overlay", "display", "block");
	Widget->SetProperty("death-overlay", "opacity", "0");
	Widget->SetProperty("victory-overlay", "display", "none");
	Widget->SetProperty("victory-overlay", "opacity", "0");
	Widget->SetProperty("death-title", "display", "block");
	Widget->SetProperty("death-title", "opacity", "0");
	Widget->SetProperty("victory-title", "display", "none");
	Widget->SetProperty("victory-score", "display", "none");
	Widget->SetProperty("victory-details", "display", "none");
	ScoreboardOverlay.Hide();
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
	VictoryOverlayElapsed = 0.0f;
	VictoryHealthRatio = std::clamp(Result.PlayerHealthRatio, 0.0f, 1.0f);

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	HideBossHealth();
	Widget->SetProperty("blood-vignette", "visibility", "hidden");
	Widget->SetProperty("blood-vignette", "opacity", "0");
	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-menu-overlay", "display", "none");
	Widget->SetProperty("death-overlay", "display", "none");
	Widget->SetProperty("death-overlay", "opacity", "0");
	Widget->SetProperty("victory-overlay", "display", "block");
	Widget->SetProperty("victory-overlay", "opacity", "0");

	// 같은 결과 UI 컨테이너를 공유하므로 다른 메뉴/타이틀을 먼저 모두 접는다.
	Widget->SetProperty("death-title", "display", "none");
	Widget->SetProperty("death-menu", "display", "none");
	Widget->SetProperty("pause-menu", "display", "none");
	Widget->SetProperty("victory-title", "display", "block");
	Widget->SetProperty("victory-title", "opacity", "0");
	Widget->SetProperty("victory-score", "display", "block");
	Widget->SetProperty("victory-score", "opacity", "0");
	Widget->SetProperty("victory-details", "display", "block");
	Widget->SetProperty("victory-details", "opacity", "0");
	ScoreboardOverlay.ConfigureSaveMode(ScoreboardEntries, ScoreboardPageSize, "Player", "이름 입력 후 저장하면 랭킹에 반영됩니다.");
	ScoreboardOverlay.Hide();

	// 표시 값은 승리 확정 시점에 고정된 Result만 사용한다.
	Widget->SetText("victory-score", FString("score: ") + std::to_string(static_cast<long long>(Result.Score)));
	Widget->SetText("victory-details", MakeVictoryDetailsText(Result));
	Widget->SetWantsMouse(false);
}

void FMusouHudPresenter::NotifyVictoryScoreSubmitted(const TArray<FMusouScoreboardEntry>& ScoreboardEntries)
{
	bVictoryScoreSubmitted = true;
	ScoreboardOverlay.NotifyScoreSubmitted(ScoreboardEntries, "저장 완료");

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	if (bVictoryScoreboardVisible)
	{
		bVictoryButtonsVisible = true;
		Widget->SetWantsMouse(true);
	}
}

void FMusouHudPresenter::NotifyVictoryScoreSaveFailed()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	ScoreboardOverlay.SetSaveStatus("저장 실패");
}

void FMusouHudPresenter::ShowPreviousScoreboardPage()
{
	ScoreboardOverlay.ShowPreviousPage();
}

void FMusouHudPresenter::ShowNextScoreboardPage()
{
	ScoreboardOverlay.ShowNextPage();
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

	if (!bCombatHudVisible)
	{
		Widget->SetProperty("blood-vignette", "opacity", "0");
		Widget->SetProperty("blood-vignette", "visibility", "hidden");
	}
	else if (BloodAlpha > 0.01f)
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

void FMusouHudPresenter::UpdateStoryDialogCutscene(float DeltaTime)
{
	if (!bStoryDialogCutsceneFadeActive || !Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	StoryDialogCutsceneFadeElapsed += DeltaTime;
	const float Alpha = SmoothStep01(StoryDialogCutsceneFadeElapsed / StoryDialogCutsceneFadeDuration);

	if (StoryDialogCutscenePreviousIndex >= 0 && StoryDialogCutscenePreviousIndex < StoryDialogCutsceneCount)
	{
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutscenePreviousIndex], "display", "block");
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutscenePreviousIndex], "opacity", MakeOpacityValue(1.0f - Alpha));
	}

	if (StoryDialogCutsceneIndex >= 0 && StoryDialogCutsceneIndex < StoryDialogCutsceneCount)
	{
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutsceneIndex], "display", "block");
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutsceneIndex], "opacity", MakeOpacityValue(Alpha));
	}

	if (Alpha < 1.0f)
	{
		return;
	}

	if (StoryDialogCutscenePreviousIndex >= 0
		&& StoryDialogCutscenePreviousIndex < StoryDialogCutsceneCount
		&& StoryDialogCutscenePreviousIndex != StoryDialogCutsceneIndex)
	{
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutscenePreviousIndex], "display", "none");
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutscenePreviousIndex], "opacity", "0");
	}

	if (StoryDialogCutsceneIndex >= 0 && StoryDialogCutsceneIndex < StoryDialogCutsceneCount)
	{
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutsceneIndex], "display", "block");
		Widget->SetProperty(StoryDialogCutsceneElementIds[StoryDialogCutsceneIndex], "opacity", "1");
	}
	else
	{
		HideStoryDialogCutscenes();
		return;
	}

	StoryDialogCutscenePreviousIndex = -1;
	StoryDialogCutsceneFadeElapsed = 0.0f;
	bStoryDialogCutsceneFadeActive = false;
}

void FMusouHudPresenter::BeginStoryDialogCutsceneTransition()
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	if (StoryDialogKind != EStoryDialogKind::Intro)
	{
		HideStoryDialogCutscenes();
		return;
	}

	const int32 NextCutsceneIndex = GetIntroDialogCutsceneIndex(StoryDialogPageIndex);
	if (NextCutsceneIndex == StoryDialogCutsceneIndex)
	{
		for (int32 Index = 0; Index < StoryDialogCutsceneCount; ++Index)
		{
			const bool bCurrentCutscene = Index == StoryDialogCutsceneIndex;
			Widget->SetProperty(StoryDialogCutsceneElementIds[Index], "display", bCurrentCutscene ? "block" : "none");
			Widget->SetProperty(StoryDialogCutsceneElementIds[Index], "opacity", bCurrentCutscene ? "1" : "0");
		}
		StoryDialogCutscenePreviousIndex = -1;
		StoryDialogCutsceneFadeElapsed = 0.0f;
		bStoryDialogCutsceneFadeActive = false;
		return;
	}

	StoryDialogCutscenePreviousIndex = StoryDialogCutsceneIndex;
	StoryDialogCutsceneIndex = NextCutsceneIndex;
	StoryDialogCutsceneFadeElapsed = 0.0f;
	bStoryDialogCutsceneFadeActive = true;

	for (int32 Index = 0; Index < StoryDialogCutsceneCount; ++Index)
	{
		const bool bShouldDisplay =
			Index == StoryDialogCutscenePreviousIndex || Index == StoryDialogCutsceneIndex;
		Widget->SetProperty(StoryDialogCutsceneElementIds[Index], "display", bShouldDisplay ? "block" : "none");
		Widget->SetProperty(StoryDialogCutsceneElementIds[Index], "opacity", Index == StoryDialogCutscenePreviousIndex ? "1" : "0");
	}

	if (StoryDialogCutscenePreviousIndex < 0 && StoryDialogCutsceneIndex < 0)
	{
		HideStoryDialogCutscenes();
	}
}

void FMusouHudPresenter::HideStoryDialogCutscenes()
{
	if (Widget && Widget->IsDocumentLoaded())
	{
		for (int32 Index = 0; Index < StoryDialogCutsceneCount; ++Index)
		{
			Widget->SetProperty(StoryDialogCutsceneElementIds[Index], "display", "none");
			Widget->SetProperty(StoryDialogCutsceneElementIds[Index], "opacity", "0");
		}
	}

	StoryDialogCutsceneIndex = -1;
	StoryDialogCutscenePreviousIndex = -1;
	StoryDialogCutsceneFadeElapsed = 0.0f;
	bStoryDialogCutsceneFadeActive = false;
}

void FMusouHudPresenter::UpdateStoryDialog(float DeltaTime)
{
	if ((!IsStoryDialogActive()) || !Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	UpdateStoryDialogCutscene(DeltaTime);

	if (bStoryDialogFadeOutActive)
	{
		StoryDialogFadeOutElapsed += DeltaTime;
		const float Alpha = 1.0f - SmoothStep01(StoryDialogFadeOutElapsed / StoryDialogFadeOutDuration);
		Widget->SetProperty("story-dialog-overlay", "display", "block");
		Widget->SetProperty("story-dialog-overlay", "opacity", MakeOpacityValue(Alpha));
		if (Alpha <= 0.0f)
		{
			bStoryDialogFadeOutActive = false;
			StoryDialogFadeOutElapsed = 0.0f;
			Widget->SetProperty("story-dialog-overlay", "display", "none");
			Widget->SetProperty("story-dialog-overlay", "opacity", "1");
			Widget->SetProperty("story-dialog-panel", "display", "block");
		}
		return;
	}

	const TArray<FString>& Pages = GetActiveStoryDialogPages();
	if (StoryDialogPageIndex < 0 || StoryDialogPageIndex >= static_cast<int32>(Pages.size()))
	{
		FinishStoryDialog();
		return;
	}

	StoryDialogElapsed += DeltaTime;

	const int32 CurrentGlyphCount = CountStoryDialogGlyphs(Pages[StoryDialogPageIndex]);
	const float PreviousProgress = StoryDialogTextProgress;
	StoryDialogTextProgress = std::min(
		static_cast<float>(CurrentGlyphCount),
		StoryDialogTextProgress + DeltaTime * StoryDialogCharsPerSecond);

	if (StoryDialogTextProgress != PreviousProgress)
	{
		RenderStoryDialogText();
	}

	const bool bPageComplete = StoryDialogTextProgress >= static_cast<float>(CurrentGlyphCount);
	const bool bShowNextIndicator = bPageComplete && StoryDialogKind != EStoryDialogKind::BossSequence;
	if (bShowNextIndicator)
	{
		const float Pulse = 0.5f + 0.5f * static_cast<float>(std::sin(StoryDialogElapsed * StoryDialogIndicatorPulseSpeed));
		Widget->SetProperty("story-dialog-next-indicator", "visibility", "visible");
		Widget->SetProperty("story-dialog-next-indicator", "opacity", MakeOpacityValue(0.45f + Pulse * 0.55f));
	}
	else
	{
		Widget->SetProperty("story-dialog-next-indicator", "visibility", "hidden");
		Widget->SetProperty("story-dialog-next-indicator", "opacity", "0");
	}
}

void FMusouHudPresenter::RenderStoryDialogText()
{
	if (StoryDialogKind == EStoryDialogKind::None || !Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	const TArray<FString>& Pages = GetActiveStoryDialogPages();
	if (StoryDialogPageIndex < 0 || StoryDialogPageIndex >= static_cast<int32>(Pages.size()))
	{
		return;
	}

	Widget->SetText("story-dialog-text", MakeStoryDialogTextRml(Pages[StoryDialogPageIndex], StoryDialogTextProgress));
}

void FMusouHudPresenter::FinishStoryDialog()
{
	const EStoryDialogKind FinishedKind = StoryDialogKind;
	StoryDialogKind = EStoryDialogKind::None;
	StoryDialogPageIndex = 0;
	StoryDialogTextProgress = 0.0f;
	StoryDialogElapsed = 0.0f;
	if (FinishedKind == EStoryDialogKind::BossSequence)
	{
		BossSequenceDialogPages.clear();
	}

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetWantsMouse(false);
	HideStoryDialogCutscenes();

	if (FinishedKind == EStoryDialogKind::Outro)
	{
		bStoryDialogFadeOutActive = true;
		StoryDialogFadeOutElapsed = 0.0f;
		Widget->SetProperty("story-dialog-panel", "display", "none");
		Widget->SetProperty("story-dialog-overlay", "display", "block");
		Widget->SetProperty("story-dialog-overlay", "opacity", "1");
		return;
	}

	Widget->SetProperty("story-dialog-overlay", "display", "none");
	Widget->SetProperty("story-dialog-overlay", "opacity", "1");
	Widget->SetProperty("story-dialog-overlay", "background-color", StoryDialogBlackOverlayColor);
	Widget->SetProperty("story-dialog-panel", "display", "block");
}

bool FMusouHudPresenter::StartStoryDialog(EStoryDialogKind InKind)
{
	StoryDialogKind = InKind;
	const TArray<FString>& Pages = GetActiveStoryDialogPages();
	if (!Widget || !Widget->IsDocumentLoaded() || Pages.empty())
	{
		StoryDialogKind = EStoryDialogKind::None;
		return false;
	}

	bStoryDialogFadeOutActive = false;
	HideStoryDialogCutscenes();
	StoryDialogPageIndex = 0;
	StoryDialogTextProgress = 0.0f;
	StoryDialogElapsed = 0.0f;
	StoryDialogFadeOutElapsed = 0.0f;

	const bool bUseSceneOverlay = InKind == EStoryDialogKind::Intro || InKind == EStoryDialogKind::Outro;
	Widget->SetProperty("story-dialog-overlay", "opacity", "1");
	Widget->SetProperty(
		"story-dialog-overlay",
		"background-color",
		bUseSceneOverlay ? StoryDialogBlackOverlayColor : StoryDialogTransparentOverlayColor);
	Widget->SetProperty("story-dialog-next-indicator", "visibility", "hidden");
	Widget->SetProperty("story-dialog-next-indicator", "opacity", "0");
	Widget->SetText("story-dialog-text", "");
	Widget->SetWantsMouse(false);
	BeginStoryDialogCutsceneTransition();
	RenderStoryDialogText();
	Widget->SetProperty("story-dialog-panel", "display", "block");
	Widget->SetProperty("story-dialog-overlay", "display", "block");
	return true;
}

const TArray<FString>& FMusouHudPresenter::GetActiveStoryDialogPages() const
{
	switch (StoryDialogKind)
	{
	case EStoryDialogKind::Intro:
		return GetIntroDialogPages();
	case EStoryDialogKind::Outro:
		return GetOutroDialogPages();
	case EStoryDialogKind::FinalBoss:
		return GetFinalBossDialogPages();
	case EStoryDialogKind::BossSequence:
		return BossSequenceDialogPages;
	default:
		break;
	}

	static const TArray<FString> EmptyPages;
	return EmptyPages;
}

void FMusouHudPresenter::UpdateDeathOverlay(float DeltaTime)
{
	if (!bDeathOverlayVisible)
	{
		return;
	}

	DeathOverlayElapsed += DeltaTime;

	const float OverlayAlpha = SmoothStep01(DeathOverlayElapsed / DeathOverlayFadeDuration);
	const float TitleAlpha = SmoothStep01((DeathOverlayElapsed - DeathTitleFadeDelay) / DeathTitleFadeDuration);

	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("death-overlay", "display", "block");
	Widget->SetProperty("death-overlay", "opacity", MakeOpacityValue(OverlayAlpha));
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

	const float OverlayAlpha = SmoothStep01(VictoryOverlayElapsed / VictoryOverlayFadeDuration);
	const float TitleAlpha = SmoothStep01((VictoryOverlayElapsed - VictoryTitleFadeDelay) / VictoryTitleFadeDuration);
	const float ScoreAlpha = SmoothStep01((VictoryOverlayElapsed - VictoryScoreFadeDelay) / VictoryScoreFadeDuration);

	// overlay 배경색은 RML에 두고, 여기서는 페이드 opacity만 갱신한다.
	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("victory-overlay", "display", "block");
	Widget->SetProperty("victory-overlay", "opacity", MakeOpacityValue(OverlayAlpha));
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
		ScoreboardOverlay.Hide();
		Widget->SetWantsMouse(false);
		return;
	}

	if (!bVictoryScoreboardVisible)
	{
		bVictoryScoreboardVisible = true;
		bVictoryButtonsVisible = true;
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "none");
		ScoreboardOverlay.Show();
		Widget->SetWantsMouse(true);

		// 스코어보드가 열린 직후 바로 이름을 입력할 수 있도록 입력창에 포커스를 준다.
		ScoreboardOverlay.FocusNameInput();
	}
}
