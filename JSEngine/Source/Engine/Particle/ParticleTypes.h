#pragma once

#include "Core/CoreMinimal.h"
#include "Core/CollisionTypes.h"

class AActor;
class UParticleSystemComponent;
class UPrimitiveComponent;
struct FParticleEmitterInstance;

enum class EParticleEmitterRenderMode : uint8
{
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};

struct FBaseParticle
{
	FVector Location = FVector::ZeroVector;
	FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector BaseVelocity = FVector::ZeroVector;
	float RelativeTime = 0.0f;
	float Lifetime = 1.0f;
	FVector Size = FVector(1.0f, 1.0f, 1.0f);
	FColor Color = FColor::White();
	float Rotation = 0.0f;
	float RotationRate = 0.0f;
	uint32 ParticleId = 0;
	uint32 Flags = 0;
	int32 CollisionCount = 0;
};

struct FParticleDataContainer
{
	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;

	void Reset()
	{
		delete[] ParticleData;
		MemBlockSize = 0;
		ParticleDataNumBytes = 0;
		ParticleIndicesNumShorts = 0;
		ParticleData = nullptr;
		ParticleIndices = nullptr;
	}
};

struct FParticleEventCollideData
{
	UParticleSystemComponent* Component = nullptr;
	FParticleEmitterInstance* EmitterInstance = nullptr;
	int32 EmitterIndex = -1;
	uint32 ParticleId = 0;
	FVector Location = FVector::ZeroVector;
	FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	UPrimitiveComponent* HitComponent = nullptr;
	AActor* HitActor = nullptr;
	float Time = 0.0f;
	FHitResult Hit;
};
