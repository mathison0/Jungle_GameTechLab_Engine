#pragma once

#include "Particle/ParticleSystem.h"

class UParticleSystemComponent;

struct FParticleEmitterInstance
{
    FParticleEmitterInstance() = default;
    ~FParticleEmitterInstance();

    void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, int32 InEmitterIndex);
    void Reset();
    void Tick(float DeltaTime, bool bAllowSpawning = true);
    void SelectLODLevel(float Distance);
    void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
                        const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr);
    void KillParticle(int32 Index);
	
	// Getter
    int32 GetActiveParticleCount() const { return ActiveParticles; }
    int32 GetMaxActiveParticleCount() const { return MaxActiveParticles; }
    int32 GetParticleStride() const { return ParticleStride; }
    int32 GetParticleSize() const { return ParticleSize; }
    int32 GetParticleMemoryBytes() const { return ParticleStorage.GetMemoryBytes(); }

	const uint8* GetParticleData() const { return ParticleStorage.ParticleData; }
    const uint16* GetParticleIndices() const { return ParticleStorage.ParticleIndices; }

	UParticleEmitter* GetTemplate() const { return SpriteTemplate; }
    UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }
    int32 GetCurrentLODLevelIndex() const { return CurrentLODLevelIndex; }
    int32 GetEmitterIndex() const { return EmitterIndex; }
    uint32 GetParticleCounter() const { return ParticleCounter; }
    FParticleEmitterRuntimeView GetRuntimeView() const;

	FBaseParticle* GetParticle(int32 ActiveIndex);
    const FBaseParticle* GetParticle(int32 ActiveIndex) const;

	UParticleSystemComponent* GetComponent() const { return Component; }
    FVector GetComponentWorldLocation() const;
	UParticleSystemComponent* GetOwningComponent() const { return Component; }


    void QueueCollisionEvent(const FParticleEventCollideData& EventData);
    void DispatchQueuedParticleEvents();
    int32 ConsumeSpawnCount(float Rate, float DeltaTime);

private:
	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;
	int32 EmitterIndex = -1;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	// 실제 데이터들, memory pool and live data
	FParticleDataContainer ParticleStorage;
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
