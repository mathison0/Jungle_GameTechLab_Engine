#pragma once

#include "GameFramework/AActor.h"
#include "Particle/ParticleTypes.h"

class UParticleSystem;

DECLARE_DELEGATE(FOnParticleEventCollide, const FParticleEventCollideData&);

UCLASS(Placeable, DisplayName = "Particle Event Manager", Category = "Effects")
class AParticleEventManager : public AActor
{
public:
	GENERATED_BODY(AParticleEventManager, AActor)

	~AParticleEventManager() override;

	void PushCollisionEvent(const FParticleEventCollideData& EventData);
	void DispatchEvents();
	const TArray<FParticleEventCollideData>& GetCollisionEvents() const { return CollisionEvents; }
	void ClearEvents() { CollisionEvents.clear(); }

	void InitDefaultComponents() override;

	FOnParticleEventCollide OnParticleCollide;

private:
	TArray<FParticleEventCollideData> CollisionEvents;
	UParticleSystem* PreviewParticleSystem = nullptr;
};
