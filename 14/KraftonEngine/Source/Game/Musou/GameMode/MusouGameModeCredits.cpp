#include "Game/Musou/GameMode/MusouGameModeCredits.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/World.h"
#include "GameFramework/Camera/CameraActor.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "Component/Camera/CameraComponent.h"
#include "Math/Vector.h"
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
	// 롤 스크롤 — 시작 top(화면 아래) → 끝 top(화면 위). CreditsDuration 동안 선형.
	constexpr float CreditsRollStartTop = 1080.0f;
	constexpr float CreditsRollEndTop   = -1180.0f;
}

AMusouGameModeCredits::AMusouGameModeCredits()
{
}

void AMusouGameModeCredits::StartMatch()
{
	AGameModeBase::StartMatch();

	ElapsedTime = 0.0f;
	AutoAttackTimer = 0.0f;
	bFinishing = false;

	// 씬에 저작된 메인 캐릭터 — base StartMatch 가 auto-possess 한 폰을 잡는다.
	Hero = nullptr;
	if (APlayerController* PC = GetPlayerController())
	{
		Hero = Cast<AMusouCharacter>(PC->GetPossessedPawn());
	}
	if (Hero)
	{
		// 크레딧 동안 죽지 않게 — 적 군체에 둘러싸여도 계속 휘두른다.
		if (UBattleComponent* Battle = Hero->GetBattleComponent())
		{
			Battle->SetInvincible(true);
		}
		// 공격 몽타주의 카메라 샷 전환을 끈다 — 고정 시네마틱 시점 유지(화면 전환 방지).
		Hero->SetCameraShotsSuppressed(true);
		// 공격 시 카메라 정면 재조준 스냅도 끈다 — 캐릭터가 엉뚱하게 휙 도는 것 방지.
		Hero->SetCameraFacingSuppressed(true);
	}

	// 플레이어 입력 차단 — 크레딧 전투는 자동 구동, 키 입력은 건너뛰기에만 쓴다.
	if (GEngine && GEngine->GetGameViewportClient())
	{
		GEngine->GetGameViewportClient()->SetInputPossessed(false);
	}

	// 씬에 배치된 시네마틱 카메라(ACameraActor)를 active 로 — 플레이어 팔로우캠 대신.
	SetupCinematicCamera();

	if (!CreditsWidget)
	{
		CreditsWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/Credits.rml");
	}

	if (CreditsWidget)
	{
		CreditsWidget->SetWantsMouse(false);
		CreditsWidget->AddToViewport(0);
		UE_LOG("[MusouGameModeCredits] Credits UI added to viewport (hero=%s)", Hero ? "yes" : "no");
	}
}

void AMusouGameModeCredits::EndPlay()
{
	if (CreditsWidget)
	{
		CreditsWidget->RemoveFromParent();
		CreditsWidget = nullptr;
	}

	AGameModeBase::EndPlay();
}

void AMusouGameModeCredits::Tick(float DeltaTime)
{
	AGameModeBase::Tick(DeltaTime);

	if (bFinishing)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	// 시네마틱 카메라 — 메인 캐릭터를 계속 바라보게 + active 유지.
	UpdateCinematicCamera();

	// 메인 캐릭터 전투 — 처음 CombatDuration(10s) 동안만 자동 공격, 이후엔 검을 집어넣고 정지.
	if (Hero)
	{
		if (ElapsedTime < CombatDuration)
		{
			// 자동 공격 — 일정 간격으로 좌클릭 공격 진입점 호출(납도면 발도→콤보).
			AutoAttackTimer -= DeltaTime;
			if (AutoAttackTimer <= 0.0f)
			{
				Hero->TriggerAutoAttack();
				AutoAttackTimer = AutoAttackInterval;
			}
		}
		else if (Hero->IsWeaponDrawn())
		{
			// 전투 종료 — 납도 후 가만히. (모션 중이면 토글이 스스로 무시, 납도 완료 시 멈춤.)
			Hero->RequestSheathe();
		}
	}

	// 롤 스크롤 — top 을 매 틱 갱신해 아래에서 위로 흐르게 한다.
	if (CreditsWidget && CreditsWidget->IsDocumentLoaded())
	{
		const float Alpha = ElapsedTime / CreditsDuration;
		const float Top = CreditsRollStartTop + (CreditsRollEndTop - CreditsRollStartTop) * Alpha;
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%dpx", static_cast<int32>(Top));
		CreditsWidget->SetProperty("credits-roll", "top", Buffer);
	}

	// 아무 키나 누르면 즉시 건너뛰기 (ESC/Enter/Space/좌클릭). 롤이 다 흐르면 자동 복귀.
	InputSystem& Input = InputSystem::Get();
	const bool bSkip = Input.GetKeyDown(VK_ESCAPE)
		|| Input.GetKeyDown(VK_RETURN)
		|| Input.GetKeyDown(VK_SPACE)
		|| Input.GetKeyDown(VK_LBUTTON);

	if (bSkip || ElapsedTime >= CreditsDuration)
	{
		FinishCredits();
	}
}

void AMusouGameModeCredits::SetupCinematicCamera()
{
	CinematicCam = nullptr;

	// 씬에 배치된 첫 ACameraActor 를 찾는다 (플레이어 폰의 카메라 컴포넌트와 별개).
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (AActor* Actor : World->GetActors())
	{
		if (ACameraActor* CamActor = Cast<ACameraActor>(Actor))
		{
			CinematicCam = CamActor->GetCameraComponent();
			break;
		}
	}

	if (!CinematicCam)
	{
		UE_LOG("[MusouGameModeCredits] 씬에 ACameraActor 없음 — 플레이어 팔로우캠으로 폴백");
		return;
	}

	// 시작 시 한 번 캐릭터를 바라보고 active 로 — 첫 프레임부터 정답 시점.
	if (Hero)
	{
		CinematicCam->LookAt(Hero->GetActorLocation() + FVector(0.0f, 0.0f, CameraLookHeight));
	}
	if (APlayerController* PC = GetPlayerController())
	{
		if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
		{
			CamMgr->SetActiveCamera(CinematicCam);
		}
	}
}

void AMusouGameModeCredits::UpdateCinematicCamera()
{
	if (!CinematicCam)
	{
		return;
	}

	// 메인 캐릭터(공격 RM 으로 조금씩 움직임)를 매 틱 추적해 프레임 유지.
	if (Hero)
	{
		CinematicCam->LookAt(Hero->GetActorLocation() + FVector(0.0f, 0.0f, CameraLookHeight));
	}

	// 무언가(플레이어 cam 등록 등)가 active 를 가로채면 다시 시네마틱 카메라로 되돌린다.
	if (APlayerController* PC = GetPlayerController())
	{
		if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
		{
			if (CamMgr->GetActiveCamera() != CinematicCam)
			{
				CamMgr->SetActiveCamera(CinematicCam);
			}
		}
	}
}

void AMusouGameModeCredits::FinishCredits()
{
	if (bFinishing)
	{
		return;
	}
	bFinishing = true;

	UE_LOG("[MusouGameModeCredits] Credits finished — returning to Intro");
	if (GEngine)
	{
		GEngine->RequestTransitionToScene("Intro");
	}
}
