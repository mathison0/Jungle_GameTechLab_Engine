#pragma once

#include "Object/Object.h"
#include "Particle/ParticleTypes.h"

struct FParticleEmitterInstance;

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule, UObject)

	virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) {}
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
