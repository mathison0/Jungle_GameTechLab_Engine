#pragma once

#include "Particle/ParticleSystem.h"

class UParticleSystemComponent;

struct FParticleEmitterInstance
{
	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;
	int32 EmitterIndex = -1;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	uint8* InstanceData = nullptr;
	int32 InstancePayloadSize = 0;
	int32 PayloadOffset = 0;
	int32 ParticleSize = sizeof(FBaseParticle);
	int32 ParticleStride = sizeof(FBaseParticle);
	int32 ActiveParticles = 0;
	uint32 ParticleCounter = 0;
	int32 MaxActiveParticles = 0;
	float SpawnFraction = 0.0f;

	FParticleEmitterInstance() = default;
	~FParticleEmitterInstance();

	void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, int32 InEmitterIndex);
	void Reset();
	void Tick(float DeltaTime);
	void SelectLODLevel(float Distance);
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
	                    const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr);
	void KillParticle(int32 Index);

	FBaseParticle* GetParticle(int32 ActiveIndex);
	const FBaseParticle* GetParticle(int32 ActiveIndex) const;
};
