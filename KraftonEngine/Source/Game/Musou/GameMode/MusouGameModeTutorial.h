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
// 하단 안내 바(Tutorial.rml)가 기본 7단계(이동/시점/공격/강공격/점프/구르기/
// 발도납도) + 심화 6단계(점프공격/점프강공격/대시공격/3연타/분기/분기후콤보)를
// 순서대로 띄우고, 단계별 조건이 충족되면 "성공" 피드백 + 짧은 쿨다운 후 다음 단계로.
//
// 감지 방식(하이브리드):
//   - 이동/대시: 실제 속도/컨텍스트 (단순 키 탭으로 통과 방지)
//   - 시점/기본 공격/점프/구르기/발도납도: 입력
//   - 점프공격/점프강공격/대시공격: 재생된 AttackId (기술이 실제 발동했는지)
//   - 3연타/분기/분기후콤보: ComboComponent 상태
//
// 마지막에 Enter 로 전투(Play) 참가, 언제든 ESC 로 Intro 복귀.
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
	bool IsStepDone(int32 Step);   // 비-const — 분기후콤보 단계가 ComboSubPhase 를 갱신
	void EnterStep(int32 Step);    // 단계 진입(상태 리셋 + 카운터 스냅샷 + 프롬프트)
	void ShowStep(int32 Step);     // 안내 바 텍스트/카운터
	void ShowCleared();            // 성공 피드백(바 초록 + "성공!")
	void FinishToScene(const char* SceneName);

	UUserWidget* TutorialWidget = nullptr;
	AMusouCharacter* Hero = nullptr;
	int32 StepIndex = 0;
	float StepTime = 0.0f;        // 현재 단계 감지 경과(초)
	bool bStepCleared = false;    // true = 성공 피드백/쿨다운 중
	float ClearTimer = 0.0f;
	int32 ComboSubPhase = 0;      // 분기후콤보 단계: 0=분기 대기, 1=새 콤보 대기
	uint32 AttackCounterAtStepStart = 0;  // 단계 시작 시 공격 카운터 — 새 기술 발동 판정
	bool bFinishing = false;

	// 기본 6(이동/발도/공격/강공격/점프/구르기) + 심화 6. StepIndex==StepCount = 완료.
	static constexpr int32 StepCount = 12;
	// 단계당 최소 감지 노출(초) — 즉시 통과/캐스케이드 방지.
	static constexpr float MinStepTime = 0.4f;
	// 성공 피드백 + 다음 단계 쿨다운(초).
	static constexpr float ClearFeedbackTime = 1.1f;
	// 이동 단계 통과 속도 임계(m/s) — 실제로 움직여야 통과.
	static constexpr float MoveSpeedThreshold = 1.5f;
};
