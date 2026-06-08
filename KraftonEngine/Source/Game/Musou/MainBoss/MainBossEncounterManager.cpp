#include "Game/Musou/MainBoss/MainBossEncounterManager.h"

#include "Audio/AudioManager.h"
#include "Component/Camera/CineCameraComponent.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Logging/Log.h"
#include "Game/Musou/Boss/BossPatternDataRegistry.h"
#include "Game/Musou/Boss/MusouBossCharacter.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/MusouGameSettings.h"
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
			StartIntroSequence();
		}
		break;
	}
	case EMainBossEncounterFlowState::IntroSequence:
		TickIntroSequence(SafeDeltaTime);
		break;
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

	StartIntroSequence();
}

void AMainBossEncounterManager::StartIntroSequence()
{
	if (!ResolveMainBoss())
	{
		return;
	}

	if (!LoadIntroSteps() || IntroSteps.empty())
	{
		StartGettingUp();
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
	if (MainBossPattern)
	{
		MainBossPattern->SetPatternEnabled(false);
	}

	StateTime = 0.0f;
	ActiveSequenceDuration = 0.0f;
	IntroStepExecuted.assign(IntroSteps.size(), 0);
	FlowState = EMainBossEncounterFlowState::IntroSequence;
	UE_LOG("[MainBossEncounter] data intro started: %s", MainBossDataId.ToString().c_str());
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
	PlayGettingUpEffects();
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
	PlayBattlecryEffects();
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

bool AMainBossEncounterManager::LoadIntroSteps()
{
	if (bIntroStepsLoaded)
	{
		return !IntroSteps.empty();
	}

	bIntroStepsLoaded = true;
	IntroSteps.clear();

	const FBossDefinition* Definition = FBossPatternDataRegistry::Get().FindBoss(MainBossDataId);
	if (!Definition)
	{
		UE_LOG("[MainBossEncounter] intro data not found: %s", MainBossDataId.ToString().c_str());
		return false;
	}

	IntroSteps = Definition->IntroSteps;
	return !IntroSteps.empty();
}

void AMainBossEncounterManager::TickIntroSequence(float DeltaTime)
{
	if (bEncounterCameraActive)
	{
		UpdateEncounterCamera();
	}

	for (int32 Index = 0; Index < static_cast<int32>(IntroSteps.size()); ++Index)
	{
		if (Index < static_cast<int32>(IntroStepExecuted.size()) && IntroStepExecuted[Index] != 0)
		{
			continue;
		}

		const FBossSequenceStep& Step = IntroSteps[Index];
		if (StateTime < Step.Time)
		{
			continue;
		}

		ExecuteIntroStep(Step);
		if (Index < static_cast<int32>(IntroStepExecuted.size()))
		{
			IntroStepExecuted[Index] = 1;
		}
	}

	if (StateTime >= GetIntroSequenceEndTime())
	{
		FinishEncounter();
	}
	(void)DeltaTime;
}

void AMainBossEncounterManager::ExecuteIntroStep(const FBossSequenceStep& Step)
{
	switch (Step.Type)
	{
	case EBossSequenceStepType::Dialogue:
		UE_LOG("[MainBossIntro] Boss: %s", Step.Text.c_str());
		break;
	case EBossSequenceStepType::PlayMontage:
		if (MainBossPattern && !Step.MontagePath.empty())
		{
			MainBossPattern->PlayEncounterSequence(Step.MontagePath, ResolvePlayer(), Step.PlayRate);
		}
		break;
	case EBossSequenceStepType::BlendCamera:
		if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
		{
			if (!ReturnCamera)
			{
				ReturnCamera = CameraManager->GetActiveCamera();
			}

			UCameraComponent* SequenceCamera = FindSequenceCameraByTag(Step.CameraTag);
			const bool bUseGeneratedCamera = SequenceCamera == nullptr;
			if (bUseGeneratedCamera)
			{
				CameraOffset = Step.CameraOffset;
				LookAtHeight = Step.LookAtHeight;
				EncounterFOV = Step.FOV;
				EnsureEncounterCamera();
				UpdateEncounterCamera();
				SequenceCamera = EncounterCamera;
			}

			if (SequenceCamera)
			{
				CameraManager->SetActiveCameraWithBlend(
					SequenceCamera,
					Step.Duration,
					EViewTargetBlendFunction::VTBlend_EaseInOut,
					ECameraRequestPriority::BossSequence,
					true);
				CameraManager->Possess(SequenceCamera);
				bEncounterCameraActive = bUseGeneratedCamera;
			}
		}
		break;
	case EBossSequenceStepType::RestoreCamera:
		RestorePlayerCamera();
		bEncounterCameraActive = false;
		break;
	case EBossSequenceStepType::CameraFadeIn:
		if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
		{
			CameraManager->StartCameraFade(1.0f, 0.0f, Step.Duration, FLinearColor::Black(), false, false);
		}
		break;
	case EBossSequenceStepType::CameraFadeOut:
		if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
		{
			CameraManager->StartCameraFade(0.0f, 1.0f, Step.Duration, FLinearColor::Black(), false, true);
		}
		break;
	case EBossSequenceStepType::CameraShake:
		if (FMusouGameSettings::Get().IsCameraShakeEnabled())
		{
			if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
			{
				if (!Step.ShakeAssetPath.empty())
				{
					CameraManager->StartCameraShakeAsset(Step.ShakeAssetPath, Step.ShakeScale);
				}
			}
		}
		break;
	case EBossSequenceStepType::PlayAudio:
		if (!Step.SoundPath.empty())
		{
			const FString Key = FString("MainBossIntro:") + Step.SoundPath;
			if (FAudioManager::Get().LoadAudio(Key, Step.SoundPath, Step.bLoop))
			{
				FAudioManager::Get().PlayAudio(Key, Step.Volume);
			}
			else
			{
				UE_LOG("[MainBossIntro] failed to load sound: %s", Step.SoundPath.c_str());
			}
		}
		break;
	case EBossSequenceStepType::LockPlayer:
		SetCinematicLock(Step.bValue);
		break;
	case EBossSequenceStepType::UnlockPlayer:
		SetCinematicLock(false);
		break;
	case EBossSequenceStepType::SetBossPatternEnabled:
		if (MainBossPattern)
		{
			MainBossPattern->SetPatternEnabled(Step.bValue);
		}
		break;
	case EBossSequenceStepType::FaceBossToPlayer:
		FaceMainBossToPlayer();
		break;
	case EBossSequenceStepType::SetInvincible:
		if (MainBossBattle)
		{
			MainBossBattle->SetInvincible(Step.bValue);
		}
		break;
	case EBossSequenceStepType::Wait:
	default:
		break;
	}
}

float AMainBossEncounterManager::GetIntroSequenceEndTime() const
{
	float EndTime = 0.0f;
	for (const FBossSequenceStep& Step : IntroSteps)
	{
		EndTime = (std::max)(EndTime, Step.Time + Step.Duration);
	}
	return EndTime;
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

UCameraComponent* AMainBossEncounterManager::FindSequenceCameraByTag(const FString& CameraTag) const
{
	if (CameraTag.empty())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FName TagName(CameraTag);
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->HasTag(TagName))
		{
			continue;
		}

		if (UCineCameraComponent* CineCamera = Actor->GetComponentByClass<UCineCameraComponent>())
		{
			return CineCamera;
		}

		if (UCameraComponent* Camera = Actor->GetComponentByClass<UCameraComponent>())
		{
			return Camera;
		}
	}

	return nullptr;
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

void AMainBossEncounterManager::PlayGettingUpEffects()
{
	if (PlayerController)
	{
		if (FMusouGameSettings::Get().IsCameraShakeEnabled())
		{
			if (APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager())
			{
				if (!GettingUpShakeAssetPath.empty())
				{
					CameraManager->StartCameraShakeAsset(GettingUpShakeAssetPath, GettingUpShakeScale);
				}
			}
		}
	}

	if (!GettingUpSoundPath.empty())
	{
		const FString Key = FString("MainBossEncounter:") + GettingUpSoundPath;
		if (FAudioManager::Get().LoadAudio(Key, GettingUpSoundPath, false))
		{
			FAudioManager::Get().PlayAudio(Key, GettingUpSoundVolume);
		}
		else
		{
			UE_LOG("[MainBossEncounter] failed to load getting up sound: %s", GettingUpSoundPath.c_str());
		}
	}
}

void AMainBossEncounterManager::PlayBattlecryEffects()
{
	if (!PlayerController || !FMusouGameSettings::Get().IsCameraShakeEnabled())
	{
		return;
	}

	if (BattlecryShakeAssetPath.empty())
	{
		return;
	}

	if (APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager())
	{
		CameraManager->StartCameraShakeAsset(BattlecryShakeAssetPath, BattlecryShakeScale);
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
