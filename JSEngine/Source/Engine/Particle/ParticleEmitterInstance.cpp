#include "Particle/ParticleEmitterInstance.h"

#include <algorithm>

#include "Particle/ParticleSystemComponent.h"

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	Reset();
}

void FParticleEmitterInstance::Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, int32 InEmitterIndex)
{
	Reset();

	SpriteTemplate = InTemplate;
	Component = InComponent;
	EmitterIndex = InEmitterIndex;

	if (SpriteTemplate)
	{
		SpriteTemplate->CacheEmitterModuleInfo();
		ParticleSize = SpriteTemplate->GetParticleSize();
		ParticleStride = ParticleSize;
		MaxActiveParticles = std::max(SpriteTemplate->GetMaxActiveParticles(), 1);
		CurrentLODLevelIndex = SpriteTemplate->SelectLODLevel(0.0f);
		CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
	}
	else
	{
		ParticleSize = sizeof(FBaseParticle);
		ParticleStride = sizeof(FBaseParticle);
		MaxActiveParticles = 1;
	}

	ParticleData = new uint8[ParticleStride * MaxActiveParticles];
	ParticleIndices = new uint16[MaxActiveParticles];

	for (int32 Index = 0; Index < MaxActiveParticles; ++Index)
	{
		ParticleIndices[Index] = static_cast<uint16>(Index);
	}
}

void FParticleEmitterInstance::Reset()
{
	delete[] ParticleData;
	delete[] ParticleIndices;
	delete[] InstanceData;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
	InstanceData = nullptr;
	InstancePayloadSize = 0;
	PayloadOffset = 0;
	ActiveParticles = 0;
	ParticleCounter = 0;
	MaxActiveParticles = 0;
	SpawnFraction = 0.0f;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = nullptr;
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (!SpriteTemplate || !Component || !ParticleData || !ParticleIndices || DeltaTime <= 0.0f)
	{
		return;
	}

	SelectLODLevel(Component->ComputeEmitterLODDistance());
	if (!CurrentLODLevel || !CurrentLODLevel->IsEnabled())
	{
		return;
	}

	int32 SpawnCount = 0;
	if (UParticleModuleSpawn* SpawnModule = CurrentLODLevel->GetSpawnModule())
	{
		SpawnCount = SpawnModule->ComputeSpawnCount(this, DeltaTime);
	}

	SpawnParticles(SpawnCount, 0.0f, SpawnCount > 0 ? DeltaTime / static_cast<float>(SpawnCount) : 0.0f,
	               Component->GetWorldLocation(), FVector::ZeroVector);

	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; )
	{
		FBaseParticle* Particle = GetParticle(ParticleIndex);
		Particle->RelativeTime += DeltaTime / std::max(Particle->Lifetime, 0.01f);
		if (Particle->RelativeTime >= 1.0f)
		{
			KillParticle(ParticleIndex);
			continue;
		}

		Particle->OldLocation = Particle->Location;
		Particle->Location += Particle->Velocity * DeltaTime;
		++ParticleIndex;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetUpdateModules())
	{
		if (Module && Module->IsEnabled())
		{
			Module->Update(this, DeltaTime);
		}
	}
}

void FParticleEmitterInstance::SelectLODLevel(float Distance)
{
	if (!SpriteTemplate)
	{
		return;
	}

	const int32 NewLODIndex = SpriteTemplate->SelectLODLevel(Distance);
	if (NewLODIndex == CurrentLODLevelIndex && CurrentLODLevel)
	{
		return;
	}

	CurrentLODLevelIndex = NewLODIndex;
	CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment,
                                              const FVector& InitialLocation, const FVector& InitialVelocity,
                                              FParticleEventInstancePayload* EventPayload)
{
	(void)EventPayload;
	if (!CurrentLODLevel || Count <= 0)
	{
		return;
	}

	for (int32 SpawnIndex = 0; SpawnIndex < Count && ActiveParticles < MaxActiveParticles; ++SpawnIndex)
	{
		const int32 ActiveIndex = ActiveParticles;
		const uint16 SlotIndex = ParticleIndices[ActiveIndex];
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleData + SlotIndex * ParticleStride);
		*Particle = FBaseParticle();

		Particle->ParticleId = ++ParticleCounter;
		Particle->Location = InitialLocation;
		Particle->OldLocation = InitialLocation;
		Particle->Velocity = InitialVelocity;
		Particle->BaseVelocity = InitialVelocity;

		const float SpawnTime = StartTime + Increment * static_cast<float>(SpawnIndex);
		for (UParticleModule* Module : CurrentLODLevel->GetSpawnModules())
		{
			if (Module && Module->IsEnabled())
			{
				Module->Spawn(this, *Particle, SpawnTime);
			}
		}

		++ActiveParticles;
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}

	const int32 LastActiveIndex = ActiveParticles - 1;
	std::swap(ParticleIndices[Index], ParticleIndices[LastActiveIndex]);
	--ActiveParticles;
}

FBaseParticle* FParticleEmitterInstance::GetParticle(int32 ActiveIndex)
{
	if (!ParticleData || !ParticleIndices || ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<FBaseParticle*>(ParticleData + ParticleIndices[ActiveIndex] * ParticleStride);
}

const FBaseParticle* FParticleEmitterInstance::GetParticle(int32 ActiveIndex) const
{
	if (!ParticleData || !ParticleIndices || ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<const FBaseParticle*>(ParticleData + ParticleIndices[ActiveIndex] * ParticleStride);
}
