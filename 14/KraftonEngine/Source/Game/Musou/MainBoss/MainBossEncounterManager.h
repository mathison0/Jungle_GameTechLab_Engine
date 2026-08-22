#pragma once

#include "Game/Musou/Boss/BossPatternTypes.h"
#include "GameFramework/AActor.h"
#include "Math/Vector.h"
#include "Object/FName.h"

#include "Source/Game/Musou/MainBoss/MainBossEncounterManager.generated.h"

class AMainBossCharacter;
class AMusouBossCharacter;
class AMusouCharacter;
class AMusouGameMode;
class APlayerController;
class UBattleComponent;
class UCameraComponent;
class UCineCameraComponent;
class UMainBossPatternComponent;
class USceneComponent;

enum class EMainBossEncounterFlowState : uint8
{
	WaitingForMiddleBoss,
	WaitingForMiddleBossRemoval,
	WaitingForDialog,
	IntroSequence,
	GettingUp,
	Battlecry,
	Complete
};

UCLASS()
class AMainBossEncounterManager : public AActor
{
public:
	GENERATED_BODY()
	AMainBossEncounterManager();
	~AMainBossEncounterManager() override = default;

	void InitDefaultComponents();
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter", DisplayName="Auto Start")
	bool bAutoStart = true;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter", DisplayName="Middle Boss Tag")
	FName MiddleBossTag = FName("WarriorBoss");

	UPROPERTY(Edit, Save, Category="Final Boss Encounter", DisplayName="Main Boss Tag")
	FName MainBossTag = FName("FinalBoss");

	UPROPERTY(Edit, Save, Category="Final Boss Encounter", DisplayName="Main Boss Data Id")
	FName MainBossDataId = FName("golem_boss");

	UPROPERTY(Edit, Save, Category="Final Boss Encounter", DisplayName="Lock Player During Cinematic")
	bool bLockPlayerDuringCinematic = true;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter", DisplayName="Hide HUD During Cinematic")
	bool bHideHudDuringCinematic = true;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Camera", DisplayName="Camera Blend In")
	float CameraBlendIn = 0.35f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Camera", DisplayName="Camera Blend Out")
	float CameraBlendOut = 0.35f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Camera", DisplayName="Camera Offset")
	FVector CameraOffset = FVector(10.5f, -2.5f, 4.0f);

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Camera", DisplayName="Look At Height")
	float LookAtHeight = 4.0f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Camera", DisplayName="FOV")
	float EncounterFOV = 0.87266463f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Effects", DisplayName="Getting Up Shake Scale")
	float GettingUpShakeScale = 1.25f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Effects", DisplayName="Getting Up Shake Asset")
	FString GettingUpShakeAssetPath = "Content/Particle/Earthquake.uasset";

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Effects", DisplayName="Getting Up Sound")
	FString GettingUpSoundPath = "Boss_Golem/earthquake.wav";

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Effects", DisplayName="Getting Up Sound Volume")
	float GettingUpSoundVolume = 1.0f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Effects", DisplayName="Battlecry Shake Scale")
	float BattlecryShakeScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Final Boss Encounter|Effects", DisplayName="Battlecry Shake Asset")
	FString BattlecryShakeAssetPath = "Content/Particle/GolemGrowl.uasset";

private:
	bool ResolveMainBoss();
	AMusouBossCharacter* FindMiddleBoss() const;
	AMainBossCharacter* FindMainBoss() const;
	AMusouCharacter* ResolvePlayer();
	AMusouGameMode* ResolveMusouGameMode() const;
	APlayerController* ResolvePlayerController() const;
	bool IsMiddleBossDead(const AMusouBossCharacter* Boss) const;
	void PrepareMainBossDormant();
	void StartFinalBossDialog();
	void StartIntroSequence();
	void StartGettingUp();
	void StartBattlecry();
	void FinishEncounter();
	bool LoadIntroSteps();
	void TickIntroSequence(float DeltaTime);
	void ExecuteIntroStep(const FBossSequenceStep& Step);
	float GetIntroSequenceEndTime() const;
	void SetCinematicLock(bool bLocked);
	void EnsureEncounterCamera();
	UCameraComponent* FindSequenceCameraByTag(const FString& CameraTag) const;
	void UpdateEncounterCamera();
	void RestorePlayerCamera();
	void FaceMainBossToPlayer();
	void PlayGettingUpEffects();
	void PlayBattlecryEffects();

	EMainBossEncounterFlowState FlowState = EMainBossEncounterFlowState::WaitingForMiddleBoss;
	AMainBossCharacter* MainBoss = nullptr;
	AMusouCharacter* Player = nullptr;
	UMainBossPatternComponent* MainBossPattern = nullptr;
	UBattleComponent* MainBossBattle = nullptr;
	APlayerController* PlayerController = nullptr;
	UCineCameraComponent* EncounterCamera = nullptr;
	UCameraComponent* ReturnCamera = nullptr;
	float StateTime = 0.0f;
	float ActiveSequenceDuration = 0.0f;
	TArray<FBossSequenceStep> IntroSteps;
	TArray<uint8> IntroStepExecuted;
	bool bMainBossPrepared = false;
	bool bIntroStepsLoaded = false;
	bool bMiddleBossDeathObserved = false;
	bool bPlayerWasLocked = false;
	bool bEncounterCameraActive = false;
};
