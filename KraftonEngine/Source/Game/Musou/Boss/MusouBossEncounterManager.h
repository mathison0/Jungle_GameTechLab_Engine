#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/Boss/BossPatternTypes.h"
#include "GameFramework/AActor.h"
#include "Math/Vector.h"

#include "Source/Game/Musou/Boss/MusouBossEncounterManager.generated.h"

class AMusouBossCharacter;
class AMusouCharacter;
class APlayerController;
class UBattleComponent;
class UBossPatternComponent;
class UCameraComponent;
class UCineCameraComponent;
class USceneComponent;

enum class EBossEncounterIntroState : uint8
{
	Waiting,
	Playing,
	Complete
};

enum class EBossSequenceKind : uint8
{
	None = 0,
	Intro,
	Phase,
	Death
};

UCLASS()
class AMusouBossEncounterManager : public AActor
{
public:
	GENERATED_BODY()
	AMusouBossEncounterManager() = default;
	~AMusouBossEncounterManager() override = default;

	void InitDefaultComponents();
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	void StartIntro();
	void FinishIntro();
	void StartDeath();
	void SetIntroSteps(TArray<FBossSequenceStep> InSteps);
	void SetDeathSteps(TArray<FBossSequenceStep> InSteps);

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Auto Start")
	bool bAutoStart = true;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Boss Id")
	FName BossId = FName("knight_boss");

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Lock Player During Intro")
	bool bLockPlayerDuringIntro = true;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Intro Duration")
	float IntroDuration = 4.0f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Camera Blend In")
	float CameraBlendIn = 0.35f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Camera Blend Out")
	float CameraBlendOut = 0.25f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Dialogue Time")
	float DialogueTime = 0.4f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Ready Montage Time")
	float ReadyMontageTime = 1.2f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Ready Montage")
	FString ReadyMontagePath = "Content/Montages/sword and shield idle (4)_mixamo_com_Montage.uasset";

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Ready Montage Play Rate")
	float ReadyMontagePlayRate = 1.0f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Ready Montage Blend In")
	float ReadyMontageBlendIn = 0.1f;

	UPROPERTY(Edit, Save, Category="Boss Intro", DisplayName="Dialogue Line")
	FString DialogueLine = "여기까지 온 건 칭찬해주지. 하지만 여기서 끝이다.";

	UPROPERTY(Edit, Save, Category="Boss Intro|Camera", DisplayName="Camera Offset")
	FVector CameraOffset = FVector(-4.5f, -3.0f, 2.0f);

	UPROPERTY(Edit, Save, Category="Boss Intro|Camera", DisplayName="Look At Height")
	float LookAtHeight = 1.5f;

	UPROPERTY(Edit, Save, Category="Boss Intro|Camera", DisplayName="Intro FOV")
	float IntroFOV = 0.87266463f;

private:
	bool ResolveParticipants();
	bool LoadSequencesFromBossDefinition();
	void BuildDefaultIntroSteps();
	void BuildDefaultDeathSteps();
	void StartSequence(EBossSequenceKind Kind, const TArray<FBossSequenceStep>& Steps);
	bool TryStartPhaseSequence();
	void FinishSequence();
	void TickActiveSequence(float DeltaTime);
	void ExecuteSequenceStep(const FBossSequenceStep& Step);
	void EnsureIntroCamera();
	void UpdateIntroCamera();
	void FaceBossToPlayer();
	float GetActiveSequenceEndTime() const;
	APlayerController* ResolvePlayerController() const;

	EBossEncounterIntroState IntroState = EBossEncounterIntroState::Waiting;
	EBossSequenceKind ActiveSequenceKind = EBossSequenceKind::None;
	TArray<FBossSequenceStep> IntroSteps;
	TArray<FBossSequenceStep> DeathSteps;
	TArray<FBossPhaseSequence> PhaseSequences;
	TArray<FBossSequenceStep> ActiveSteps;
	TArray<uint8> StepExecuted;
	TArray<FName> TriggeredPhaseIds;
	AMusouBossCharacter* Boss = nullptr;
	AMusouCharacter* Player = nullptr;
	APlayerController* PlayerController = nullptr;
	UBattleComponent* BossBattle = nullptr;
	UBossPatternComponent* BossPattern = nullptr;
	UCineCameraComponent* IntroCamera = nullptr;
	UCameraComponent* ReturnCamera = nullptr;
	float SequenceTime = 0.0f;
	bool bIntroCameraActive = false;
	bool bPlayerWasLocked = false;
	bool bDeathSequenceStarted = false;
};
