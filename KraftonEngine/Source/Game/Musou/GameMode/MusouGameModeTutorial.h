#pragma once

#include "GameFramework/GameMode/GameModeBase.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Game/Musou/GameMode/MusouGameModeTutorial.generated.h"

class UUserWidget;
class AMusouCharacter;

// ============================================================
// AMusouGameModeTutorial — 단계별 조작 튜토리얼 씬 게임모드.
//
// 연습 씬(Play 전투 콘텐츠 복제)에서 플레이어가 직접 조작하며 배운다.
// 하단 안내 바(Tutorial.rml)가 'WASD 이동 → 시점 → 공격 → 강공격 → 점프 →
// 구르기 → 발도/납도' 순서로 프롬프트를 띄우고, 해당 입력을 감지하면 다음
// 단계로 넘어간다. 마지막에 Enter 로 전투(Play) 참가, 언제든 ESC 로 Intro 복귀.
//
// 플레이어는 입력 점유(직접 플레이) + 무적(학습 중 사망 방지).
// 씬 지정: Content/Scene/Tutorial.Scene → WorldSettings.GameMode = "AMusouGameModeTutorial"
// ============================================================
UCLASS()
class AMusouGameModeTutorial : public AGameModeBase
{
public:
	GENERATED_BODY()
	AMusouGameModeTutorial();
	~AMusouGameModeTutorial() override = default;

	void StartMatch() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;

private:
	bool IsStepInputDone(int32 Step) const;  // 해당 단계의 조작 입력이 감지됐는가
	void ShowStep(int32 Step);               // 안내 바 텍스트/카운터 갱신
	void FinishToScene(const char* SceneName);

	UUserWidget* TutorialWidget = nullptr;
	AMusouCharacter* Hero = nullptr;
	int32 StepIndex = 0;
	float StepTime = 0.0f;     // 현재 단계 노출 경과(초) — 즉시 캐스케이드 방지
	bool bFinishing = false;

	// 조작 단계 수 (이동/시점/공격/강공격/점프/구르기/발도납도). StepIndex==StepCount = 완료.
	static constexpr int32 StepCount = 7;
	// 단계당 최소 노출 시간(초) — 프롬프트를 읽을 여유 + 한 입력이 여러 단계 통과 방지.
	static constexpr float MinStepTime = 0.5f;
};
