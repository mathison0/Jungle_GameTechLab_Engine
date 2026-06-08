#pragma once

#include "GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/Musou/GameMode/MusouGameModeCredits.generated.h"

class UUserWidget;

// ============================================================
// AMusouGameModeCredits — 아웃트로(엔딩 크레딧) 씬 게임모드.
//
// 승리 후 victory 오버레이의 "그만두기"가 Credits 씬으로 전환하면, 이 모드가
// Content/UI/Credits.rml 스크롤 크레딧을 띄우고 일정 시간 뒤 자동으로 Intro 로
// 복귀한다. 아무 키나 누르면 즉시 건너뛴다. 게임플레이/입력 점유 없음.
//
// 씬 지정: Content/Scene/Credits.Scene → WorldSettings.GameMode = "AMusouGameModeCredits"
// ============================================================
UCLASS()
class AMusouGameModeCredits : public AGameModeBase
{
public:
	GENERATED_BODY()
	AMusouGameModeCredits();
	~AMusouGameModeCredits() override = default;

	void StartMatch() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;

private:
	void FinishCredits();

	UUserWidget* CreditsWidget = nullptr;
	float ElapsedTime = 0.0f;
	bool bFinishing = false;

	// 크레딧 롤(Credits.rml 의 credits-scroll 16s)이 화면 밖으로 빠지는 시간 + 여유.
	static constexpr float CreditsDuration = 16.5f;
};
