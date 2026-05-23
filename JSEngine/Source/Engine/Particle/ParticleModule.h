#pragma once

#include "Object/Object.h"
#include "Particle/ParticleTypes.h"

struct FParticleEmitterInstance;

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule, UObject)

	// Function : Apply module behavior when a particle is spawned
	// input : Owner, Particle, SpawnTime
	// Owner : emitter instance that owns the particle
	// Particle : particle being initialized
	// SpawnTime : relative spawn time within this tick
	// output : Default implementation has no effect
	virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) {}

	// Function : Apply module behavior during emitter update
	// input : Owner, DeltaTime
	// Owner : emitter instance that owns active particles
	// DeltaTime : elapsed time for this simulation step
	// output : Default implementation has no effect
	virtual void Update(FParticleEmitterInstance* Owner, float DeltaTime) {}

	bool IsEnabled() const { return bEnabled; }
	bool IsSpawnModule() const { return bSpawnModule; }
	bool IsUpdateModule() const { return bUpdateModule; }

protected:
	UPROPERTY(DisplayName = "Enabled")
	bool bEnabled = true;

	UPROPERTY(DisplayName = "Spawn Module")
	bool bSpawnModule = false;

	UPROPERTY(DisplayName = "Update Module")
	bool bUpdateModule = false;
};
