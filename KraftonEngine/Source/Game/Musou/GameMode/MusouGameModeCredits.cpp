#include "Game/Musou/GameMode/MusouGameModeCredits.h"

#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

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
	bFinishing = false;

	if (!CreditsWidget)
	{
		CreditsWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/Credits.rml");
	}

	if (CreditsWidget)
	{
		CreditsWidget->SetWantsMouse(false);
		CreditsWidget->AddToViewport(0);
		UE_LOG("[MusouGameModeCredits] Credits UI added to viewport");
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
