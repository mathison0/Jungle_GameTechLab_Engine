#include "Game/Musou/GameMode/MusouGameModeTutorial.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/ComboComponent.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/World.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"
#include "Viewport/GameViewportClient.h"

#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>  // VK_*

namespace
{
	// 각 단계 안내 문구 (인덱스 = StepIndex). StepCount 개.
	// 시작이 납도 상태라 좌클릭 공격 전에 발도(X)부터 배운다. (시점 단계는 제외)
	constexpr const char* StepPrompts[] = {
		"WASD 로 이동해 보세요",
		"X 로 칼을 뽑으세요 (발도)",
		"좌클릭으로 공격해 보세요",
		"우클릭으로 강공격을 해 보세요",
		"Space 로 점프해 보세요",
		"Shift 로 구르기(회피)를 해 보세요",
		"점프 중 좌클릭 - 점프 공격",
		"점프 중 우클릭 - 점프 강공격",
		"달리면서 좌클릭 - 대시 공격",
		"좌클릭 3연타 - 콤보",
		"좌좌 후 우클릭 - 콤보 분기",
		"좌좌우 후 다시 좌좌좌 - 분기 후 콤보",
	};
}

AMusouGameModeTutorial::AMusouGameModeTutorial()
{
}

void AMusouGameModeTutorial::StartMatch()
{
	// 베이스가 PlayerController spawn + 첫 Pawn auto-possess 를 수행한다.
	AGameModeBase::StartMatch();

	bFinishing = false;

	// 플레이어 입력 점유 — 튜토리얼은 직접 플레이하며 배운다.
	if (GEngine && GEngine->GetGameViewportClient())
	{
		GEngine->GetGameViewportClient()->SetInputPossessed(true);
	}

	// 메인 캐릭터 — 학습 중 사망 방지로 무적.
	Hero = nullptr;
	if (APlayerController* PC = GetPlayerController())
	{
		Hero = Cast<AMusouCharacter>(PC->GetPossessedPawn());
	}
	if (Hero)
	{
		if (UBattleComponent* Battle = Hero->GetBattleComponent())
		{
			Battle->SetInvincible(true);
		}
	}

	if (!TutorialWidget)
	{
		TutorialWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/Tutorial.rml");
	}
	if (TutorialWidget)
	{
		TutorialWidget->SetWantsMouse(false);
		TutorialWidget->AddToViewport(0);
	}

	EnterStep(0);
	UE_LOG("[MusouGameModeTutorial] Tutorial started (hero=%s)", Hero ? "yes" : "no");
}

void AMusouGameModeTutorial::EndPlay()
{
	if (TutorialWidget)
	{
		TutorialWidget->RemoveFromParent();
		TutorialWidget = nullptr;
	}

	AGameModeBase::EndPlay();
}

void AMusouGameModeTutorial::Tick(float DeltaTime)
{
	AGameModeBase::Tick(DeltaTime);

	if (bFinishing)
	{
		return;
	}

	InputSystem& Input = InputSystem::Get();

	// 언제든 ESC 로 Intro 복귀.
	if (Input.GetKeyDown(VK_ESCAPE))
	{
		FinishToScene("Intro");
		return;
	}

	if (StepIndex >= StepCount)
	{
		// 완료 — Enter/Space 로 전투(Play) 참가.
		if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
		{
			FinishToScene("Play");
		}
		return;
	}

	if (bStepCleared)
	{
		// 성공 피드백 + 쿨다운 — 끝나면 다음 단계로.
		ClearTimer -= DeltaTime;
		if (ClearTimer <= 0.0f)
		{
			EnterStep(StepIndex + 1);
		}
		return;
	}

	// 감지 — 최소 노출 시간 뒤 조건 충족이면 성공 처리.
	StepTime += DeltaTime;
	if (StepTime >= MinStepTime && IsStepDone(StepIndex))
	{
		bStepCleared = true;
		ClearTimer = ClearFeedbackTime;
		ShowCleared();
	}
}

bool AMusouGameModeTutorial::IsStepDone(int32 Step)
{
	InputSystem& Input = InputSystem::Get();
	const bool bNewAttack = Hero && (Hero->GetAttackPlayCounter() != AttackCounterAtStepStart);
	UComboComponent* Combo = Hero ? Hero->GetComboComponent() : nullptr;

	switch (Step)
	{
	case 0: // 이동 — 실제 속도 (단순 키 탭 방지)
		return Hero && Hero->GetPlanarSpeed() > MoveSpeedThreshold;
	case 1: // 발도 — 칼이 실제로 뽑힌 상태가 됐는가 (X 입력 → 발도 완료)
		return Hero && Hero->IsWeaponDrawn();
	case 2: // 공격 — 좌클릭
		return Input.GetKeyDown(VK_LBUTTON);
	case 3: // 강공격 — 우클릭
		return Input.GetKeyDown(VK_RBUTTON);
	case 4: // 점프 — Space
		return Input.GetKeyDown(VK_SPACE);
	case 5: // 구르기 — Shift
		return Input.GetKeyDown(VK_SHIFT);
	case 6: // 점프 공격 — jump_attack 몽타주 발동
		return bNewAttack && Hero->GetLastPlayedAttackId() == "jump_attack";
	case 7: // 점프 강공격 — jump_heavy 발동
		return bNewAttack && Hero->GetLastPlayedAttackId() == "jump_heavy";
	case 8: // 대시 공격 — dash_attack 발동(이동 중 공격으로만 진입)
		return bNewAttack && Hero->GetLastPlayedAttackId() == "dash_attack";
	case 9: // 3연타 콤보 — 콤보 단수 3 도달
		return Combo && Combo->GetComboStep() >= 3;
	case 10: // 콤보 분기 — 분기 발동
		return Combo && Combo->IsBranchActive();
	case 11: // 분기 후 콤보 — 분기(좌좌우) 후 새 콤보(좌좌좌) 3단
		if (!Combo)
		{
			return false;
		}
		if (ComboSubPhase == 0)
		{
			if (Combo->IsBranchActive())
			{
				ComboSubPhase = 1;
				if (TutorialWidget && TutorialWidget->IsDocumentLoaded())
				{
					TutorialWidget->SetText("tutorial-text", "좋아요! 이어서 좌좌좌 콤보");
				}
			}
			return false;
		}
		// 분기 종료 후 새 콤보가 3단에 도달
		return !Combo->IsBranchActive() && Combo->GetComboStep() >= 3;
	default:
		return false;
	}
}

void AMusouGameModeTutorial::EnterStep(int32 Step)
{
	StepIndex = Step;
	StepTime = 0.0f;
	bStepCleared = false;
	ComboSubPhase = 0;
	AttackCounterAtStepStart = Hero ? Hero->GetAttackPlayCounter() : 0;
	ShowStep(Step);
}

void AMusouGameModeTutorial::ShowStep(int32 Step)
{
	if (!TutorialWidget || !TutorialWidget->IsDocumentLoaded())
	{
		return;
	}

	TutorialWidget->SetClass("tutorial-bar", "cleared", false);

	if (Step < StepCount)
	{
		char Counter[16] = {};
		std::snprintf(Counter, sizeof(Counter), "%d / %d", Step + 1, StepCount);
		TutorialWidget->SetText("tutorial-step", Counter);
		TutorialWidget->SetText("tutorial-text", StepPrompts[Step]);
		TutorialWidget->SetText("tutorial-hint", "ESC: 건너뛰기");
	}
	else
	{
		TutorialWidget->SetText("tutorial-step", "완료");
		TutorialWidget->SetText("tutorial-text", "튜토리얼 완료! Enter 로 전투에 참가하세요");
		TutorialWidget->SetText("tutorial-hint", "Enter: 전투 참가   /   ESC: 메인으로");
	}
}

void AMusouGameModeTutorial::ShowCleared()
{
	if (!TutorialWidget || !TutorialWidget->IsDocumentLoaded())
	{
		return;
	}

	// 단계 카운터(숫자)는 그대로 두고 바를 초록으로 + "성공!" 만 — 폰트에 없는 글리프 미사용.
	TutorialWidget->SetClass("tutorial-bar", "cleared", true);
	TutorialWidget->SetText("tutorial-text", "성공!");
	TutorialWidget->SetText("tutorial-hint", "");
}

void AMusouGameModeTutorial::FinishToScene(const char* SceneName)
{
	if (bFinishing)
	{
		return;
	}
	bFinishing = true;

	UE_LOG("[MusouGameModeTutorial] Tutorial done -> %s", SceneName);
	if (GEngine)
	{
		GEngine->RequestTransitionToScene(SceneName);
	}
}
