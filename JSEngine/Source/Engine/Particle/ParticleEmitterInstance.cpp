#include "Particle/ParticleEmitterInstance.h"

#include <algorithm>

#include "Particle/ParticleSystemComponent.h"

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	Reset();
}

// Function : Initialize emitter instance from emitter template and owning component
// input : InTemplate, InComponent, InEmitterIndex
// InTemplate : emitter asset that owns LOD levels and particle modules
// InComponent : particle system component that owns this emitter instance
// InEmitterIndex : index of this emitter inside the particle system
// output : Particle buffers, particle indices, and current LOD state are initialized
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

// Function : Release particle instance memory and reset runtime state
// input : None
// output : Particle buffers are released and instance counters return to the default state
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

// Function : Advance emitter simulation by delta time
// input : DeltaTime
// DeltaTime : elapsed time for this simulation step
// output : New particles are spawned, active particles are updated, and expired particles are removed
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

// Function : Select LOD level from current emitter distance
// input : Distance
// Distance : distance from the emitter component to the active camera
// output : CurrentLODLevelIndex and CurrentLODLevel are updated when the selected LOD changes
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

// Function : Spawn particles into available active slots
// input : Count, StartTime, Increment, InitialLocation, InitialVelocity, EventPayload
// Count : number of particles requested for spawn
// StartTime : spawn time assigned to the first particle
// Increment : time offset added between spawned particles
// InitialLocation : base world location before spawn modules modify the particle
// InitialVelocity : base velocity before spawn modules modify the particle
// EventPayload : optional event payload passed from event-driven spawning
// output : Active particle slots are initialized and spawn modules are applied
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

// Function : Remove active particle by swapping it with the last active particle
// input : Index
// Index : active particle index to remove
// output : ActiveParticles is decreased and particle index storage remains compact
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

// Function : Get mutable particle data by active index
// input : ActiveIndex
// ActiveIndex : active particle index in the compact active list
// output : Pointer to particle data, or nullptr when the index is invalid
FBaseParticle* FParticleEmitterInstance::GetParticle(int32 ActiveIndex)
{
	if (!ParticleData || !ParticleIndices || ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<FBaseParticle*>(ParticleData + ParticleIndices[ActiveIndex] * ParticleStride);
}

// Function : Get read-only particle data by active index
// input : ActiveIndex
// ActiveIndex : active particle index in the compact active list
// output : Const pointer to particle data, or nullptr when the index is invalid
const FBaseParticle* FParticleEmitterInstance::GetParticle(int32 ActiveIndex) const
{
	if (!ParticleData || !ParticleIndices || ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<const FBaseParticle*>(ParticleData + ParticleIndices[ActiveIndex] * ParticleStride);
}
