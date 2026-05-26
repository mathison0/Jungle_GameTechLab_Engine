#pragma once

#include "GameFramework/AActor.h"
#include "Particle/ParticleTypes.h"

DECLARE_DELEGATE(FOnParticleEventCollide, const FParticleEventCollideData&);

UCLASS(Placeable, DisplayName = "Particle Event Manager", Category = "Effects")
class AParticleEventManager : public AActor
{
public:
	GENERATED_BODY(AParticleEventManager, AActor)

	void PushCollisionEvent(const FParticleEventCollideData& EventData);
	void DispatchCollisionEvents(const TArray<FParticleEventCollideData>& EventDataList);
	void DispatchEvents();
	const TArray<FParticleEventCollideData>& GetCollisionEvents() const { return CollisionEvents; }
	void ClearEvents() { CollisionEvents.clear(); }

	void InitDefaultComponents() override;

	FOnParticleEventCollide OnParticleCollide;

private:
	TArray<FParticleEventCollideData> CollisionEvents;
};
