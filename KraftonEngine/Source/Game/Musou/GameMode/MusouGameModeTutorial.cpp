#include "Game/Musou/GameMode/MusouGameModeTutorial.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
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
	constexpr const char* StepPrompts[] = {
		"WASD 로 이동해 보세요",
		"마우스를 움직여 시점을 돌려 보세요",
		"좌클릭으로 공격해 보세요",
		"우클릭으로 강공격을 해 보세요",
		"Space 로 점프해 보세요",
		"Shift 로 구르기(회피)를 해 보세요",
		"X 로 발도 / 납도를 해 보세요",
	};
}

AMusouGameModeTutorial::AMusouGameModeTutorial()
{
}

void AMusouGameModeTutorial::StartMatch()
{
	// 베이스가 PlayerController spawn + 첫 Pawn auto-possess 를 수행한다.
	AGameModeBase::StartMatch();

	StepIndex = 0;
	StepTime = 0.0f;
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
		ShowStep(0);
		UE_LOG("[MusouGameModeTutorial] Tutorial UI added (hero=%s)", Hero ? "yes" : "no");
	}
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

	StepTime += DeltaTime;

	if (StepIndex < StepCount)
	{
		// 최소 노출 시간 뒤, 해당 단계 입력이 감지되면 다음 단계로.
		if (StepTime >= MinStepTime && IsStepInputDone(StepIndex))
		{
			++StepIndex;
			StepTime = 0.0f;
			ShowStep(StepIndex);
		}
	}
	else
	{
		// 완료 단계 — Enter/Space 로 전투(Play) 참가.
		if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
		{
			FinishToScene("Play");
		}
	}
}

bool AMusouGameModeTutorial::IsStepInputDone(int32 Step) const
{
	InputSystem& Input = InputSystem::Get();
	switch (Step)
	{
	case 0: // 이동 — WASD 중 하나라도 눌림(유지 포함)
		return Input.GetKey('W') || Input.GetKey('A') || Input.GetKey('S') || Input.GetKey('D');
	case 1: // 시점 — 마우스 이동
		return Input.MouseMoved();
	case 2: // 공격 — 좌클릭
		return Input.GetKeyDown(VK_LBUTTON);
	case 3: // 강공격 — 우클릭
		return Input.GetKeyDown(VK_RBUTTON);
	case 4: // 점프 — Space
		return Input.GetKeyDown(VK_SPACE);
	case 5: // 구르기 — Shift
		return Input.GetKeyDown(VK_SHIFT);
	case 6: // 발도/납도 — X
		return Input.GetKeyDown('X');
	default:
		return false;
	}
}

void AMusouGameModeTutorial::ShowStep(int32 Step)
{
	if (!TutorialWidget || !TutorialWidget->IsDocumentLoaded())
	{
		return;
	}

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
		// 완료.
		TutorialWidget->SetText("tutorial-step", "완료");
		TutorialWidget->SetText("tutorial-text", "튜토리얼 완료! Enter 로 전투에 참가하세요");
		TutorialWidget->SetText("tutorial-hint", "Enter: 전투 참가   /   ESC: 메인으로");
	}
}

void AMusouGameModeTutorial::FinishToScene(const char* SceneName)
{
	if (bFinishing)
	{
		return;
	}
	bFinishing = true;

	UE_LOG("[MusouGameModeTutorial] Tutorial done — -> %s", SceneName);
	if (GEngine)
	{
		GEngine->RequestTransitionToScene(SceneName);
	}
}
