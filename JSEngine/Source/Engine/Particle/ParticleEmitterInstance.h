#pragma once

#include "Particle/ParticleSystem.h"

class UParticleSystemComponent;

struct FParticleEmitterInstance
{
    FParticleEmitterInstance() = default;
    ~FParticleEmitterInstance();

    void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, int32 InEmitterIndex);
    void Reset();
    void Tick(float DeltaTime);
    void SelectLODLevel(float Distance);
    void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
                        const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr);
    void KillParticle(int32 Index);
	
	// Getter
    int32 GetActiveParticleCount() const { return ActiveParticles; }
    int32 GetMaxActiveParticles() const { return MaxActiveParticles; }
    int32 GetParticleStride() const { return ParticleStride; }
    int32 GetParticleSize() const { return ParticleSize; }

	const uint8* GetParticleData() const { return ParticleData; }
    const uint16* GetParticleIndices() const { return ParticleIndices; }

	UParticleEmitter* GetTemplate() const { return SpriteTemplate; }
    UParticleLODLevel* GetCurrLODLevel() const { return CurrentLODLevel; }
    int32 GetCurrLODLevelIndex() const { return CurrentLODLevelIndex; }
    int32 GetEmitterIndex() const { return EmitterIndex; }
    uint32 GetParticleCounter() const { return ParticleCounter; }
    FParticleEmitterRuntimeView GetRuntimeView() const;

	FBaseParticle* GetParticle(int32 ActiveIndex);
    const FBaseParticle* GetParticle(int32 ActiveIndex) const;

	UParticleSystemComponent* GetComponent() const { return Component; }
    float GetSpawnFraction() const { return SpawnFraction; }
	// Setter
    void SetSpawnFraction(float InSpawnFraction);

private:
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




};
