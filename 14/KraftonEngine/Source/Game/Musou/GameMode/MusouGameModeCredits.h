#pragma once

#include "GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/Musou/GameMode/MusouGameModeCredits.generated.h"

class UUserWidget;
class AMusouCharacter;
class UCameraComponent;

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
	void SetupCinematicCamera();   // 씬에 배치된 ACameraActor 를 찾아 active 로.
	void UpdateCinematicCamera();  // 매 틱 메인 캐릭터를 바라보게 + active 유지.

	UUserWidget* CreditsWidget = nullptr;
	AMusouCharacter* Hero = nullptr;   // 씬에 저작된 메인 캐릭터 (auto-possess) — 자동 공격 구동 대상
	UCameraComponent* CinematicCam = nullptr;  // 씬에 배치된 ACameraActor 의 카메라 — 크레딧 시점
	float ElapsedTime = 0.0f;
	float AutoAttackTimer = 0.0f;
	bool bFinishing = false;

	// 카메라가 바라보는 높이 오프셋(m) — 캐릭터 액터 위치 기준 상단(상체/머리).
	static constexpr float CameraLookHeight = 1.0f;

	// 크레딧 롤이 화면 밖으로 빠지는 시간 + 여유.
	static constexpr float CreditsDuration = 16.5f;
	// 메인 캐릭터 자동 공격 간격(초) — 콤보가 이어지도록 짧게.
	static constexpr float AutoAttackInterval = 0.85f;
	// 자동 전투 지속(초). 이 시간이 지나면 검을 집어넣고(납도) 가만히 선다.
	static constexpr float CombatDuration = 10.0f;
};
