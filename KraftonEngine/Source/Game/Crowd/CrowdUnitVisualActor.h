#pragma once

#include "GameFramework/AActor.h"
#include "Game/Crowd/CrowdUnitTypes.h"

#include "Source/Game/Crowd/CrowdUnitVisualActor.generated.h"

class UClass;
class ULargeScaleUnitManagerComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS()
class ACrowdUnitVisualActor : public AActor
{
public:
	GENERATED_BODY()

	ACrowdUnitVisualActor();

	void InitializeVisual(
		ULargeScaleUnitManagerComponent* InManager,
		USkeletalMesh* InMesh,
		const TArray<UMaterialInterface*>& InMaterials,
		UClass* InAnimClass,
		const FCrowdMeleeAnimationSet& InMeleeAnimationSet);
	void ApplyRenderData(const FUnitRenderData& InRenderData);
	void DeactivateVisual();
	void Tick(float DeltaTime) override;

	bool IsVisualActive() const { return bVisualActive; }
	FUnitHandle GetUnitHandle() const { return UnitHandle; }
	EUnitState GetCrowdState() const { return UnitState; }
	EUnitCombatType GetCrowdCombatType() const { return UnitCombatType; }
	ECrowdUnitLOD GetCrowdLOD() const { return UnitLOD; }
	float GetCrowdSpeed() const { return Speed; }
	const FVector& GetCrowdVelocity() const { return Velocity; }
	float GetCrowdCircleAroundDirectionSign() const { return CircleAroundDirectionSign; }
	float GetCrowdAnimTime() const { return AnimTime; }
	bool IsCrowdKnockDownGettingUp() const { return bKnockDownGettingUp; }
	bool ShouldLogCrowdAnimationState() const;

	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }

private:
	USkeletalMeshComponent* EnsureMeshComponent();
	void BuildDynamicMaterials();
	void ApplyHitFlashAmount(float Amount);

private:
	USkeletalMeshComponent* MeshComponent = nullptr;
	ULargeScaleUnitManagerComponent* Manager = nullptr;
	FUnitHandle UnitHandle;
	EUnitState UnitState = EUnitState::Idle;
	EUnitCombatType UnitCombatType = EUnitCombatType::Melee;
	ECrowdUnitLOD UnitLOD = ECrowdUnitLOD::Full;
	FVector Velocity = FVector::ZeroVector;
	float Speed = 0.0f;
	float CircleAroundDirectionSign = 1.0f;
	float AnimTime = 0.0f;
	bool bKnockDownGettingUp = false;
	bool bVisualActive = false;

	USkeletalMesh* CurrentMesh = nullptr;
	TArray<UMaterialInterface*> CurrentMaterials;
	TArray<UMaterialInstanceDynamic*> DynamicMaterials;
	UClass* CurrentAnimClass = nullptr;
	FCrowdMeleeAnimationSet CurrentMeleeAnimationSet;
};
