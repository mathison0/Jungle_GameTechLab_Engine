#include "Game/Musou/MainBoss/MainBossEncounterManager.h"

#include "Component/Camera/CineCameraComponent.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Logging/Log.h"
#include "Game/Musou/Boss/MusouBossCharacter.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/MainBoss/MainBossCharacter.h"
#include "Game/Musou/MainBoss/MainBossPatternComponent.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>

namespace
{
	float YawFromDirectionXY(const FVector& Direction)
	{
		return std::atan2(Direction.Y, Direction.X) * (180.0f / 3.14159265f);
	}
}

AMainBossEncounterManager::AMainBossEncounterManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AMainBossEncounterManager::InitDefaultComponents()
{
	if (!GetRootComponent())
	{
		SetRootComponent(AddComponent<USceneComponent>());
	}
}

void AMainBossEncounterManager::BeginPlay()
{
	Super::BeginPlay();
	InitDefaultComponents();

	if (ResolveMainBoss())
	{
		PrepareMainBossDormant();
	}
}

void AMainBossEncounterManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bAutoStart || FlowState == EMainBossEncounterFlowState::Complete)
	{
		return;
	}

	if (!bMainBossPrepared && ResolveMainBoss())
	{
		PrepareMainBossDormant();
	}

	if (bEncounterCameraActive)
	{
		UpdateEncounterCamera();
	}

	const float SafeDeltaTime = (std::max)(DeltaTime, 0.0f);
	StateTime += SafeDeltaTime;

	switch (FlowState)
	{
	case EMainBossEncounterFlowState::WaitingForMiddleBoss:
	{
		if (AMusouBossCharacter* MiddleBoss = FindMiddleBoss())
		{
			if (IsMiddleBossDead(MiddleBoss))
			{
				bMiddleBossDeathObserved = true;
				FlowState = EMainBossEncounterFlowState::WaitingForMiddleBossRemoval;
				StateTime = 0.0f;
			}
		}
		else if (bMiddleBossDeathObserved)
		{
			StartFinalBossDialog();
		}
		break;
	}
	case EMainBossEncounterFlowState::WaitingForMiddleBossRemoval:
		if (!FindMiddleBoss())
		{
			StartFinalBossDialog();
		}
		break;
	case EMainBossEncounterFlowState::WaitingForDialog:
	{
		AMusouGameMode* GameMode = ResolveMusouGameMode();
		if (!GameMode || !GameMode->IsFinalBossEncounterDialogActive())
		{
			StartGettingUp();
		}
		break;
	}
	case EMainBossEncounterFlowState::GettingUp:
		if (StateTime >= ActiveSequenceDuration)
		{
			StartBattlecry();
		}
		break;
	case EMainBossEncounterFlowState::Battlecry:
		if (StateTime >= ActiveSequenceDuration)
		{
			FinishEncounter();
		}
		break;
	case EMainBossEncounterFlowState::Complete:
	default:
		break;
	}
}

bool AMainBossEncounterManager::ResolveMainBoss()
{
	if (!MainBoss)
	{
		MainBoss = FindMainBoss();
	}

	MainBossPattern = MainBoss ? MainBoss->GetComponentByClass<UMainBossPatternComponent>() : nullptr;
	MainBossBattle = MainBoss ? MainBoss->GetComponentByClass<UBattleComponent>() : nullptr;
	return MainBoss && MainBossPattern;
}

AMusouBossCharacter* AMainBossEncounterManager::FindMiddleBoss() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->HasTag(MiddleBossTag))
		{
			if (AMusouBossCharacter* Boss = Cast<AMusouBossCharacter>(Actor))
			{
				return Boss;
			}
		}
	}

	return nullptr;
}

AMainBossCharacter* AMainBossEncounterManager::FindMainBoss() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->HasTag(MainBossTag))
		{
			if (AMainBossCharacter* Boss = Cast<AMainBossCharacter>(Actor))
			{
				return Boss;
			}
		}
	}

	return nullptr;
}

AMusouCharacter* AMainBossEncounterManager::ResolvePlayer()
{
	if (Player)
	{
		return Player;
	}

	PlayerController = ResolvePlayerController();
	if (PlayerController)
	{
		Player = Cast<AMusouCharacter>(PlayerController->GetPossessedPawn());
	}

	if (!Player)
	{
		if (UWorld* World = GetWorld())
		{
			for (AActor* Actor : World->GetActors())
			{
				if (AMusouCharacter* MusouCharacter = Cast<AMusouCharacter>(Actor))
				{
					Player = MusouCharacter;
					break;
				}
			}
		}
	}

	return Player;
}

AMusouGameMode* AMainBossEncounterManager::ResolveMusouGameMode() const
{
	UWorld* World = GetWorld();
	return World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
}

APlayerController* AMainBossEncounterManager::ResolvePlayerController() const
{
	UWorld* World = GetWorld();
	return World ? World->GetFirstPlayerController() : nullptr;
}

bool AMainBossEncounterManager::IsMiddleBossDead(const AMusouBossCharacter* Boss) const
{
	const UBattleComponent* Battle = Boss ? Boss->GetComponentByClass<UBattleComponent>() : nullptr;
	return Battle && Battle->IsDead();
}

void AMainBossEncounterManager::PrepareMainBossDormant()
{
	if (!ResolveMainBoss())
	{
		return;
	}

	MainBossPattern->PrepareDormantPose();
	if (MainBossBattle)
	{
		MainBossBattle->SetInvincible(true);
	}
	bMainBossPrepared = true;
}

void AMainBossEncounterManager::StartFinalBossDialog()
{
	StateTime = 0.0f;
	FlowState = EMainBossEncounterFlowState::WaitingForDialog;

	AMusouGameMode* GameMode = ResolveMusouGameMode();
	if (GameMode && GameMode->TryStartFinalBossEncounterDialog())
	{
		UE_LOG("[MainBossEncounter] final boss dialog started");
		return;
	}

	StartGettingUp();
}

void AMainBossEncounterManager::StartGettingUp()
{
	if (!ResolveMainBoss())
	{
		return;
	}

	ResolvePlayer();
	SetCinematicLock(true);
	if (AMusouGameMode* GameMode = ResolveMusouGameMode())
	{
		if (bHideHudDuringCinematic)
		{
			GameMode->SetHudVisible(false);
		}
	}

	if (MainBossBattle)
	{
		MainBossBattle->SetInvincible(true);
	}

	FaceMainBossToPlayer();
	EnsureEncounterCamera();
	UpdateEncounterCamera();
	if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
	{
		if (!ReturnCamera)
		{
			ReturnCamera = CameraManager->GetActiveCamera();
		}

		if (EncounterCamera)
		{
			CameraManager->SetActiveCameraWithBlend(
				EncounterCamera,
				CameraBlendIn,
				EViewTargetBlendFunction::VTBlend_EaseInOut,
				ECameraRequestPriority::BossSequence,
				true);
			CameraManager->Possess(EncounterCamera);
			bEncounterCameraActive = true;
		}
	}

	ActiveSequenceDuration = MainBossPattern->PlayEncounterGettingUp(Player);
	StateTime = 0.0f;
	FlowState = EMainBossEncounterFlowState::GettingUp;
	UE_LOG("[MainBossEncounter] getting up started");
}

void AMainBossEncounterManager::StartBattlecry()
{
	if (!ResolveMainBoss())
	{
		return;
	}

	FaceMainBossToPlayer();
	ActiveSequenceDuration = MainBossPattern->PlayEncounterBattlecry(Player);
	StateTime = 0.0f;
	FlowState = EMainBossEncounterFlowState::Battlecry;
	UE_LOG("[MainBossEncounter] battlecry started");
}

void AMainBossEncounterManager::FinishEncounter()
{
	if (!ResolveMainBoss())
	{
		return;
	}

	if (MainBossBattle)
	{
		MainBossBattle->SetInvincible(false);
	}

	MainBossPattern->StartEncounterCombat();
	RestorePlayerCamera();
	SetCinematicLock(false);
	if (AMusouGameMode* GameMode = ResolveMusouGameMode())
	{
		if (bHideHudDuringCinematic)
		{
			GameMode->SetHudVisible(true);
		}
	}

	bEncounterCameraActive = false;
	FlowState = EMainBossEncounterFlowState::Complete;
	UE_LOG("[MainBossEncounter] combat started");
}

void AMainBossEncounterManager::SetCinematicLock(bool bLocked)
{
	if (!bLockPlayerDuringCinematic)
	{
		return;
	}

	PlayerController = ResolvePlayerController();
	ResolvePlayer();

	if (bLocked)
	{
		if (PlayerController && PlayerController->GetPossessedPawn())
		{
			PlayerController->UnPossess();
			bPlayerWasLocked = true;
		}
		return;
	}

	if (bPlayerWasLocked && PlayerController && Player)
	{
		PlayerController->Possess(Player);
		bPlayerWasLocked = false;
	}
}

void AMainBossEncounterManager::EnsureEncounterCamera()
{
	if (EncounterCamera)
	{
		return;
	}

	EncounterCamera = AddComponent<UCineCameraComponent>();
	if (!GetRootComponent())
	{
		SetRootComponent(EncounterCamera);
	}
	else if (GetRootComponent() != EncounterCamera)
	{
		EncounterCamera->AttachToComponent(GetRootComponent());
	}
}

void AMainBossEncounterManager::UpdateEncounterCamera()
{
	if (!EncounterCamera || !MainBoss || !ResolvePlayer())
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - MainBoss->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (ToPlayer.IsNearlyZero())
	{
		ToPlayer = MainBoss->GetActorForward();
		ToPlayer.Z = 0.0f;
	}
	if (ToPlayer.IsNearlyZero())
	{
		ToPlayer = FVector(1.0f, 0.0f, 0.0f);
	}

	const FVector Forward = ToPlayer.Normalized();
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	const FVector LookAt = MainBoss->GetActorLocation() + FVector(0.0f, 0.0f, LookAtHeight);
	const FVector CameraLocation = LookAt
		+ Forward * CameraOffset.X
		+ Right * CameraOffset.Y
		+ FVector(0.0f, 0.0f, CameraOffset.Z);

	EncounterCamera->SetWorldLocation(CameraLocation);
	EncounterCamera->LookAt(LookAt);
	EncounterCamera->SetFOV(EncounterFOV);
}

void AMainBossEncounterManager::RestorePlayerCamera()
{
	if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
	{
		if (ReturnCamera)
		{
			CameraManager->SetActiveCameraWithBlend(
				ReturnCamera,
				CameraBlendOut,
				EViewTargetBlendFunction::VTBlend_EaseInOut,
				ECameraRequestPriority::BossSequence,
				true);
			CameraManager->Possess(ReturnCamera);
		}
		CameraManager->ReleaseCameraRequestPriority(ECameraRequestPriority::BossSequence);
	}
}

void AMainBossEncounterManager::FaceMainBossToPlayer()
{
	if (!MainBoss || !ResolvePlayer())
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - MainBoss->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (!ToPlayer.IsNearlyZero())
	{
		MainBoss->SetActorRotation(FRotator(0.0f, YawFromDirectionXY(ToPlayer), 0.0f));
	}
}
