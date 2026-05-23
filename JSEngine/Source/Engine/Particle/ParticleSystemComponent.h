#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleEmitterInstance.h"

DECLARE_DELEGATE(FOnParticleCollide, const FParticleEventCollideData&);

UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent() = default;
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }
	TArray<FParticleEventCollideData>& GetPendingCollisionEvents() { return PendingCollisionEvents; }
	const TArray<FParticleEventCollideData>& GetPendingCollisionEvents() const { return PendingCollisionEvents; }

	void RecreateEmitterInstances();
	void ClearEmitterInstances();
	float ComputeEmitterLODDistance() const;
	void QueueCollisionEvent(const FParticleEventCollideData& EventData);
	void DispatchQueuedParticleEvents();

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_ParticleSystem; }
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	bool SupportsOutline() const override { return false; }

	FOnParticleCollide OnParticleCollide;

	int32 GetTotalActiveParticleCount() const;

	int32 GetEmitterInstanceCount() const;
	FParticleEmitterInstance* GetEmitterInstance(int32 Index);
	const FParticleEmitterInstance* GetEmitterInstance(int32 Index) const;

protected:
	void TickComponent(float DeltaTime) override;

private:
	UPROPERTY(DisplayName = "Template")
	UParticleSystem* Template = nullptr;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FParticleEventCollideData> PendingCollisionEvents;
};
