#pragma once

#include "GameFramework/AActor.h"
#include "Game/Crowd/CrowdUnitTypes.h"

#include "Source/Game/Crowd/CrowdUnitVisualActor.generated.h"

class UClass;
class ULargeScaleUnitManagerComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS()
class ACrowdUnitVisualActor : public AActor
{
public:
	GENERATED_BODY()

	ACrowdUnitVisualActor();

	void InitializeVisual(ULargeScaleUnitManagerComponent* InManager, USkeletalMesh* InMesh, UClass* InAnimClass);
	void ApplyRenderData(const FUnitRenderData& InRenderData);
	void DeactivateVisual();
	void Tick(float DeltaTime) override;

	bool IsVisualActive() const { return bVisualActive; }
	FUnitHandle GetUnitHandle() const { return UnitHandle; }
	EUnitState GetCrowdState() const { return UnitState; }
	float GetCrowdSpeed() const { return Speed; }

	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }

private:
	USkeletalMeshComponent* EnsureMeshComponent();

private:
	USkeletalMeshComponent* MeshComponent = nullptr;
	ULargeScaleUnitManagerComponent* Manager = nullptr;
	FUnitHandle UnitHandle;
	EUnitState UnitState = EUnitState::Idle;
	float Speed = 0.0f;
	bool bVisualActive = false;

	USkeletalMesh* CurrentMesh = nullptr;
	UClass* CurrentAnimClass = nullptr;
};
