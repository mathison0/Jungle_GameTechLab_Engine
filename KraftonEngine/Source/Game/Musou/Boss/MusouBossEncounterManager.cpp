#include "Game/Musou/Boss/MusouBossEncounterManager.h"

#include "Audio/AudioManager.h"
#include "Component/Camera/CineCameraComponent.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Logging/Log.h"
#include "Game/Musou/Boss/BossPatternComponent.h"
#include "Game/Musou/Boss/BossPatternDataRegistry.h"
#include "Game/Musou/Boss/MusouBossCharacter.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/MusouGameSettings.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	float YawFromDirectionXY(const FVector& Direction)
	{
		return std::atan2(Direction.Y, Direction.X) * (180.0f / 3.14159265f);
	}

	AMusouGameMode* ResolveMusouGameMode(UWorld* World)
	{
		return World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	}

	void ShowBossSequenceDialogue(UWorld* World, const FString& Text)
	{
		if (Text.empty())
		{
			return;
		}

		if (AMusouGameMode* GameMode = ResolveMusouGameMode(World))
		{
			GameMode->ShowBossSequenceDialogue(Text);
		}
	}

	void HideBossSequenceDialogue(UWorld* World)
	{
		if (AMusouGameMode* GameMode = ResolveMusouGameMode(World))
		{
			GameMode->HideBossSequenceDialogue();
		}
	}
}

void AMusouBossEncounterManager::InitDefaultComponents()
{
	if (!GetRootComponent())
	{
		SetRootComponent(AddComponent<USceneComponent>());
	}
}

void AMusouBossEncounterManager::BeginPlay()
{
	Super::BeginPlay();
	InitDefaultComponents();
}

void AMusouBossEncounterManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IntroState == EBossEncounterIntroState::Waiting)
	{
		if (bAutoStart)
		{
			StartIntro();
		}
		return;
	}

	if (!bDeathSequenceStarted && BossBattle && BossBattle->IsDead())
	{
		StartDeath();
	}

	if (ActiveSequenceKind != EBossSequenceKind::None)
	{
		SequenceTime += (std::max)(DeltaTime, 0.0f);
		TickActiveSequence(DeltaTime);
		return;
	}

	TryStartPhaseSequence();
}

void AMusouBossEncounterManager::StartIntro()
{
	if (IntroState != EBossEncounterIntroState::Waiting)
	{
		return;
	}

	if (!ResolveParticipants())
	{
		return;
	}

	if (!LoadSequencesFromBossDefinition())
	{
		BuildDefaultIntroSteps();
		BuildDefaultDeathSteps();
	}
	if (IntroSteps.empty())
	{
		BuildDefaultIntroSteps();
	}
	if (DeathSteps.empty())
	{
		BuildDefaultDeathSteps();
	}

	IntroState = EBossEncounterIntroState::Playing;
	if (AMusouGameMode* GameMode = ResolveMusouGameMode(GetWorld()))
	{
		GameMode->SetHudVisible(false);
	}
	StartSequence(EBossSequenceKind::Intro, IntroSteps);
	UE_LOG("[BossIntro] started");
}

void AMusouBossEncounterManager::FinishIntro()
{
	if (IntroState != EBossEncounterIntroState::Playing && ActiveSequenceKind != EBossSequenceKind::Intro)
	{
		return;
	}

	if (bPlayerWasLocked && PlayerController && Player)
	{
		PlayerController->Possess(Player);
	}

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
	}

	if (BossPattern)
	{
		BossPattern->SetPatternEnabled(true);
	}

	bIntroCameraActive = false;
	ActiveSequenceKind = EBossSequenceKind::None;
	ActiveSteps.clear();
	IntroState = EBossEncounterIntroState::Complete;
	if (AMusouGameMode* GameMode = ResolveMusouGameMode(GetWorld()))
	{
		GameMode->HideBossSequenceDialogue();
		GameMode->SetHudVisible(true);
	}
	if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
	{
		CameraManager->ReleaseCameraRequestPriority(ECameraRequestPriority::BossSequence);
	}
	UE_LOG("[BossIntro] finished");
}

void AMusouBossEncounterManager::StartDeath()
{
	if (bDeathSequenceStarted)
	{
		return;
	}

	if (!ResolveParticipants())
	{
		return;
	}

	if (!LoadSequencesFromBossDefinition() && DeathSteps.empty())
	{
		BuildDefaultDeathSteps();
	}
	if (DeathSteps.empty())
	{
		BuildDefaultDeathSteps();
	}

	bDeathSequenceStarted = true;
	StartSequence(EBossSequenceKind::Death, DeathSteps);
	UE_LOG("[BossDeath] started");
}

void AMusouBossEncounterManager::SetIntroSteps(TArray<FBossSequenceStep> InSteps)
{
	IntroSteps = std::move(InSteps);
	StepExecuted.clear();
}

void AMusouBossEncounterManager::SetDeathSteps(TArray<FBossSequenceStep> InSteps)
{
	DeathSteps = std::move(InSteps);
	StepExecuted.clear();
}

bool AMusouBossEncounterManager::ResolveParticipants()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	PlayerController = ResolvePlayerController();
	if (!Player && PlayerController)
	{
		Player = Cast<AMusouCharacter>(PlayerController->GetPossessedPawn());
	}

	if (!Player || !Boss)
	{
		for (AActor* Actor : World->GetActors())
		{
			if (!Player)
			{
				Player = Cast<AMusouCharacter>(Actor);
			}
			if (!Boss)
			{
				Boss = Cast<AMusouBossCharacter>(Actor);
			}
			if (Player && Boss)
			{
				break;
			}
		}
	}

	BossPattern = Boss ? Boss->GetComponentByClass<UBossPatternComponent>() : nullptr;
	BossBattle = Boss ? Boss->GetComponentByClass<UBattleComponent>() : nullptr;
	if (Boss && Boss->BossId.IsValid())
	{
		BossId = Boss->BossId;
	}
	return Player && Boss && BossPattern;
}

bool AMusouBossEncounterManager::LoadSequencesFromBossDefinition()
{
	FBossPatternDataRegistry& Registry = FBossPatternDataRegistry::Get();
	Registry.EnsureFresh();

	if (const FBossDefinition* Definition = Registry.FindBoss(BossId))
	{
		bool bLoadedAny = false;
		if (!Definition->IntroSteps.empty())
		{
			IntroSteps = Definition->IntroSteps;
			UE_LOG("[BossIntro] loaded %d intro steps for BossId '%s'",
				static_cast<int32>(IntroSteps.size()),
				BossId.ToString().c_str());
			bLoadedAny = true;
		}
		if (!Definition->DeathSteps.empty())
		{
			DeathSteps = Definition->DeathSteps;
			UE_LOG("[BossDeath] loaded %d death steps for BossId '%s'",
				static_cast<int32>(DeathSteps.size()),
				BossId.ToString().c_str());
			bLoadedAny = true;
		}
		if (!Definition->PhaseSequences.empty())
		{
			PhaseSequences = Definition->PhaseSequences;
			UE_LOG("[BossPhase] loaded %d phase sequences for BossId '%s'",
				static_cast<int32>(PhaseSequences.size()),
				BossId.ToString().c_str());
			bLoadedAny = true;
		}
		return bLoadedAny;
	}

	return false;
}

void AMusouBossEncounterManager::BuildDefaultIntroSteps()
{
	IntroSteps.clear();

	FBossSequenceStep LockStep;
	LockStep.Type = EBossSequenceStepType::LockPlayer;
	LockStep.Time = 0.0f;
	LockStep.bValue = bLockPlayerDuringIntro;
	IntroSteps.push_back(LockStep);

	FBossSequenceStep DisablePatternStep;
	DisablePatternStep.Type = EBossSequenceStepType::SetBossPatternEnabled;
	DisablePatternStep.Time = 0.0f;
	DisablePatternStep.bValue = false;
	IntroSteps.push_back(DisablePatternStep);

	FBossSequenceStep FaceStep;
	FaceStep.Type = EBossSequenceStepType::FaceBossToPlayer;
	FaceStep.Time = 0.0f;
	IntroSteps.push_back(FaceStep);

	FBossSequenceStep CameraStep;
	CameraStep.Type = EBossSequenceStepType::BlendCamera;
	CameraStep.Time = 0.0f;
	CameraStep.Duration = CameraBlendIn;
	CameraStep.CameraOffset = CameraOffset;
	CameraStep.LookAtHeight = LookAtHeight;
	CameraStep.FOV = IntroFOV;
	IntroSteps.push_back(CameraStep);

	FBossSequenceStep SoundStep;
	SoundStep.Type = EBossSequenceStepType::PlayAudio;
	SoundStep.Time = 0.35f;
	SoundStep.SoundPath = "Boss_Warrior/boss_warrior_00.wav";
	SoundStep.Volume = 1.0f;
	IntroSteps.push_back(SoundStep);

	FBossSequenceStep DialogueStep;
	DialogueStep.Type = EBossSequenceStepType::Dialogue;
	DialogueStep.Time = DialogueTime;
	DialogueStep.Text = DialogueLine;
	IntroSteps.push_back(DialogueStep);

	FBossSequenceStep MontageStep;
	MontageStep.Type = EBossSequenceStepType::PlayMontage;
	MontageStep.Time = ReadyMontageTime;
	MontageStep.MontagePath = ReadyMontagePath;
	MontageStep.PlayRate = ReadyMontagePlayRate;
	MontageStep.BlendIn = ReadyMontageBlendIn;
	IntroSteps.push_back(MontageStep);

	FBossSequenceStep RestoreCameraStep;
	RestoreCameraStep.Type = EBossSequenceStepType::RestoreCamera;
	RestoreCameraStep.Time = (std::max)(IntroDuration - CameraBlendOut, 0.0f);
	RestoreCameraStep.Duration = CameraBlendOut;
	IntroSteps.push_back(RestoreCameraStep);

	FBossSequenceStep UnlockStep;
	UnlockStep.Type = EBossSequenceStepType::UnlockPlayer;
	UnlockStep.Time = IntroDuration;
	IntroSteps.push_back(UnlockStep);

	FBossSequenceStep EnablePatternStep;
	EnablePatternStep.Type = EBossSequenceStepType::SetBossPatternEnabled;
	EnablePatternStep.Time = IntroDuration;
	EnablePatternStep.bValue = true;
	IntroSteps.push_back(EnablePatternStep);
}

void AMusouBossEncounterManager::BuildDefaultDeathSteps()
{
	DeathSteps.clear();

	FBossSequenceStep DisablePatternStep;
	DisablePatternStep.Type = EBossSequenceStepType::SetBossPatternEnabled;
	DisablePatternStep.Time = 0.0f;
	DisablePatternStep.bValue = false;
	DeathSteps.push_back(DisablePatternStep);

	FBossSequenceStep StopMovementStep;
	StopMovementStep.Type = EBossSequenceStepType::StopMovement;
	StopMovementStep.Time = 0.0f;
	DeathSteps.push_back(StopMovementStep);

	FBossSequenceStep InvincibleStep;
	InvincibleStep.Type = EBossSequenceStepType::SetInvincible;
	InvincibleStep.Time = 0.0f;
	InvincibleStep.bValue = true;
	DeathSteps.push_back(InvincibleStep);

	FBossSequenceStep SoundStep;
	SoundStep.Type = EBossSequenceStepType::PlayAudio;
	SoundStep.Time = 0.05f;
	SoundStep.SoundPath = "Boss_Warrior/boss_warrior_00.wav";
	SoundStep.Volume = 1.0f;
	DeathSteps.push_back(SoundStep);
}

void AMusouBossEncounterManager::StartSequence(EBossSequenceKind Kind, const TArray<FBossSequenceStep>& Steps)
{
	ActiveSequenceKind = Kind;
	ActiveSteps = Steps;
	StepExecuted.assign(ActiveSteps.size(), 0);
	SequenceTime = 0.0f;
	bIntroCameraActive = false;
	HideBossSequenceDialogue(GetWorld());
	if (Kind == EBossSequenceKind::Phase)
	{
		if (AMusouGameMode* GameMode = ResolveMusouGameMode(GetWorld()))
		{
			GameMode->SetHudVisible(false);
		}
	}
	if (Kind == EBossSequenceKind::Intro)
	{
		bPlayerWasLocked = false;
	}
}

bool AMusouBossEncounterManager::TryStartPhaseSequence()
{
	if (!BossBattle || BossBattle->IsDead() || PhaseSequences.empty())
	{
		return false;
	}

	const float HealthRatio = BossBattle->GetHealthRatio();
	for (const FBossPhaseSequence& Phase : PhaseSequences)
	{
		if (!Phase.Id.IsValid() || Phase.Steps.empty() || HealthRatio > Phase.HealthRatio)
		{
			continue;
		}

		bool bAlreadyTriggered = false;
		for (const FName& TriggeredId : TriggeredPhaseIds)
		{
			if (TriggeredId == Phase.Id)
			{
				bAlreadyTriggered = true;
				break;
			}
		}
		if (Phase.bOnce && bAlreadyTriggered)
		{
			continue;
		}

		if (Phase.bOnce)
		{
			TriggeredPhaseIds.push_back(Phase.Id);
		}

		StartSequence(EBossSequenceKind::Phase, Phase.Steps);
		UE_LOG("[BossPhase] started '%s' at health ratio %.3f",
			Phase.Id.ToString().c_str(),
			HealthRatio);
		return true;
	}

	return false;
}

void AMusouBossEncounterManager::FinishSequence()
{
	if (ActiveSequenceKind == EBossSequenceKind::Intro)
	{
		FinishIntro();
		return;
	}

	if (ActiveSequenceKind == EBossSequenceKind::Death)
	{
		bIntroCameraActive = false;
		ActiveSequenceKind = EBossSequenceKind::None;
		ActiveSteps.clear();
		HideBossSequenceDialogue(GetWorld());
		if (AMusouGameMode* GameMode = ResolveMusouGameMode(GetWorld()))
		{
			GameMode->SetHudVisible(true);
		}
		if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
		{
			CameraManager->ReleaseCameraRequestPriority(ECameraRequestPriority::BossSequence);
		}
		UE_LOG("[BossDeath] finished");
		return;
	}

	if (ActiveSequenceKind == EBossSequenceKind::Phase)
	{
		bIntroCameraActive = false;
		ActiveSequenceKind = EBossSequenceKind::None;
		ActiveSteps.clear();
		HideBossSequenceDialogue(GetWorld());
		if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
		{
			CameraManager->ReleaseCameraRequestPriority(ECameraRequestPriority::BossSequence);
		}
		UE_LOG("[BossPhase] finished");
		return;
	}

	ActiveSequenceKind = EBossSequenceKind::None;
	ActiveSteps.clear();
	HideBossSequenceDialogue(GetWorld());
	if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
	{
		CameraManager->ReleaseCameraRequestPriority(ECameraRequestPriority::BossSequence);
	}
}

void AMusouBossEncounterManager::TickActiveSequence(float DeltaTime)
{
	if (bIntroCameraActive)
	{
		UpdateIntroCamera();
	}

	for (int32 Index = 0; Index < static_cast<int32>(ActiveSteps.size()); ++Index)
	{
		if (Index < static_cast<int32>(StepExecuted.size()) && StepExecuted[Index] != 0)
		{
			continue;
		}

		const FBossSequenceStep& Step = ActiveSteps[Index];
		if (SequenceTime < Step.Time)
		{
			continue;
		}

		ExecuteSequenceStep(Step);
		if (Index < static_cast<int32>(StepExecuted.size()))
		{
			StepExecuted[Index] = 1;
		}
	}

	if (SequenceTime >= GetActiveSequenceEndTime())
	{
		FinishSequence();
	}
	(void)DeltaTime;
}

void AMusouBossEncounterManager::ExecuteSequenceStep(const FBossSequenceStep& Step)
{
	switch (Step.Type)
	{
	case EBossSequenceStepType::Dialogue:
		ShowBossSequenceDialogue(GetWorld(), Step.Text);
		UE_LOG("[BossSequence] Boss: %s", Step.Text.c_str());
		break;
	case EBossSequenceStepType::PlayMontage:
		if (BossPattern)
		{
			BossPattern->PlayBossMontage(Step.MontagePath, Step.PlayRate, Step.BlendIn);
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
				IntroFOV = Step.FOV;
				EnsureIntroCamera();
				UpdateIntroCamera();
				SequenceCamera = IntroCamera;
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
				bIntroCameraActive = bUseGeneratedCamera;
			}
		}
		break;
	case EBossSequenceStepType::RestoreCamera:
		if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr)
		{
			if (ReturnCamera)
			{
				CameraManager->SetActiveCameraWithBlend(
					ReturnCamera,
					Step.Duration,
					EViewTargetBlendFunction::VTBlend_EaseInOut,
					ECameraRequestPriority::BossSequence,
					true);
				CameraManager->Possess(ReturnCamera);
			}
		}
		bIntroCameraActive = false;
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
				else
				{
					CameraManager->StartCameraShake<UWaveOscillatorCameraShake>(Step.ShakeScale);
				}
			}
		}
		break;
	case EBossSequenceStepType::WarningRim:
		if (Boss)
		{
			if (UHitFlashComponent* HitFlash = Boss->GetComponentByClass<UHitFlashComponent>())
			{
				HitFlash->PlayFlash(
					Step.Color,
					Step.Duration,
					Step.Intensity,
					Step.RimIntensity,
					Step.RimPower,
					Step.FillAmount);
			}
		}
		break;
	case EBossSequenceStepType::SetParticleEnabled:
		SetBossParticlesEnabled(Step.bValue);
		break;
	case EBossSequenceStepType::PlayAudio:
		if (!Step.SoundPath.empty())
		{
			const FString Key = FString("BossSequence:") + Step.SoundPath;
			if (FAudioManager::Get().LoadAudio(Key, Step.SoundPath, Step.bLoop))
			{
				FAudioManager::Get().PlayAudio(Key, Step.Volume);
			}
			else
			{
				UE_LOG("[BossSequence] LoadAudio failed: %s", Step.SoundPath.c_str());
			}
		}
		break;
	case EBossSequenceStepType::LockPlayer:
		if (Step.bValue && PlayerController && PlayerController->GetPossessedPawn())
		{
			PlayerController->UnPossess();
			bPlayerWasLocked = true;
		}
		break;
	case EBossSequenceStepType::UnlockPlayer:
		if (bPlayerWasLocked && PlayerController && Player)
		{
			PlayerController->Possess(Player);
			bPlayerWasLocked = false;
		}
		break;
	case EBossSequenceStepType::SetBossPatternEnabled:
		if (BossPattern)
		{
			BossPattern->SetPatternEnabled(Step.bValue);
		}
		break;
	case EBossSequenceStepType::FaceBossToPlayer:
		FaceBossToPlayer();
		break;
	case EBossSequenceStepType::StopMovement:
		if (Boss)
		{
			if (UCharacterMovementComponent* Movement = Boss->GetComponentByClass<UCharacterMovementComponent>())
			{
				Movement->MaxWalkSpeed = 0.0f;
			}
		}
		break;
	case EBossSequenceStepType::SetInvincible:
		if (BossBattle)
		{
			BossBattle->SetInvincible(Step.bValue);
		}
		break;
	case EBossSequenceStepType::DestroyActor:
		if (UWorld* World = GetWorld())
		{
			if (Boss)
			{
				World->DestroyActor(Boss);
			}
		}
		break;
	case EBossSequenceStepType::Wait:
	default:
		break;
	}
}

void AMusouBossEncounterManager::SetBossParticlesEnabled(bool bEnabled)
{
	if (!Boss)
	{
		return;
	}

	for (UActorComponent* Component : Boss->GetComponents())
	{
		UParticleSystemComponent* Particle = Cast<UParticleSystemComponent>(Component);
		if (!Particle)
		{
			continue;
		}

		if (bEnabled)
		{
			Particle->ResetSystem();
			Particle->Activate();
			Particle->SetEmitterSpawningEnabled(true);
		}
		else
		{
			Particle->SetEmitterSpawningEnabled(false);
			Particle->Deactivate();
		}
	}
}

void AMusouBossEncounterManager::EnsureIntroCamera()
{
	if (IntroCamera)
	{
		return;
	}

	IntroCamera = AddComponent<UCineCameraComponent>();
	if (!GetRootComponent())
	{
		SetRootComponent(IntroCamera);
	}
	else if (GetRootComponent() != IntroCamera)
	{
		IntroCamera->AttachToComponent(GetRootComponent());
	}
}

UCameraComponent* AMusouBossEncounterManager::FindSequenceCameraByTag(const FString& CameraTag) const
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

	UE_LOG("[BossSequence] camera_tag '%s' not found - using generated camera", CameraTag.c_str());
	return nullptr;
}

void AMusouBossEncounterManager::UpdateIntroCamera()
{
	if (!IntroCamera || !Boss || !Player)
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - Boss->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (ToPlayer.IsNearlyZero())
	{
		ToPlayer = Boss->GetActorForward();
		ToPlayer.Z = 0.0f;
	}
	if (ToPlayer.IsNearlyZero())
	{
		ToPlayer = FVector(1.0f, 0.0f, 0.0f);
	}

	const FVector Forward = ToPlayer.Normalized();
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	const FVector LookAt = Boss->GetActorLocation() + FVector(0.0f, 0.0f, LookAtHeight);
	const FVector CameraLocation = LookAt
		+ Forward * CameraOffset.X
		+ Right * CameraOffset.Y
		+ FVector(0.0f, 0.0f, CameraOffset.Z);

	IntroCamera->SetWorldLocation(CameraLocation);
	IntroCamera->LookAt(LookAt);
	IntroCamera->SetFOV(IntroFOV);
}

void AMusouBossEncounterManager::FaceBossToPlayer()
{
	if (!Boss || !Player)
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - Boss->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (!ToPlayer.IsNearlyZero())
	{
		Boss->SetActorRotation(FRotator(0.0f, YawFromDirectionXY(ToPlayer), 0.0f));
	}
}

float AMusouBossEncounterManager::GetActiveSequenceEndTime() const
{
	float EndTime = 0.0f;
	for (const FBossSequenceStep& Step : ActiveSteps)
	{
		EndTime = (std::max)(EndTime, Step.Time + Step.Duration);
	}
	return EndTime;
}

APlayerController* AMusouBossEncounterManager::ResolvePlayerController() const
{
	UWorld* World = GetWorld();
	return World ? World->GetFirstPlayerController() : nullptr;
}
