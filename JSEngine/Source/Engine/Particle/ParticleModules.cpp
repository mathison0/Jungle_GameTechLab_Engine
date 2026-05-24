#include "Particle/ParticleModules.h"

#include <algorithm>

#include "Core/Random/EngineRandom.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleSystemComponent.h"


// Function : Generate random float inside range
// input : Min, Max
// Min : minimum random value
// Max : maximum random value
// output : Random float between Min and Max
static float RandomRange(float Min, float Max)
{
    return FEngineRandom::Get().RandomFloat(Min, Max);
}

// Function : Generate random vector inside per-axis range
// input : Min, Max
// Min : minimum vector value per axis
// Max : maximum vector value per axis
// output : Random vector with each axis sampled between Min and Max
static FVector RandomRangeVector(const FVector& Min, const FVector& Max)
{
    return FVector(
        RandomRange(Min.X, Max.X),
        RandomRange(Min.Y, Max.Y),
        RandomRange(Min.Z, Max.Z));
}


UParticleModuleRequired::UParticleModuleRequired()
{
    bSpawnModule = true;
}

// Function : Apply required default particle values at spawn time
// input : Owner, Particle, SpawnTime
// Owner : emitter instance that owns the particle
// Particle : particle being initialized
// SpawnTime : relative spawn time within this tick
// output : Particle time, lifetime, size, and color receive required defaults
void UParticleModuleRequired::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.RelativeTime = 0.0f;
    Particle.Lifetime = std::max(Particle.Lifetime, 0.01f);
    Particle.Size = FVector(1.0f, 1.0f, 1.0f);
    Particle.Color = FColor::White();
}

UParticleModuleSpawn::UParticleModuleSpawn()
{
    bSpawnModule = false;
}

// Function : Compute number of particles to spawn for this tick
// input : Owner, DeltaTime
// Owner : emitter instance that stores fractional spawn remainder
// DeltaTime : elapsed time for this simulation step
// output : Integer spawn count and updated Owner SpawnFraction remainder
int32 UParticleModuleSpawn::ComputeSpawnCount(FParticleEmitterInstance* Owner, float DeltaTime)
{
    if (!Owner || Rate <= 0.0f || DeltaTime <= 0.0f)
    {
        return 0;
    }

	return Owner->ConsumeSpawnCount(Rate, DeltaTime);
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
    bSpawnModule = true;
}

// Function : Assign randomized lifetime to spawned particle
// input : Owner, Particle, SpawnTime
// Owner : emitter instance that owns the particle
// Particle : particle receiving lifetime value
// SpawnTime : relative spawn time within this tick
// output : Particle lifetime is set between LifetimeMin and LifetimeMax
void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.Lifetime = std::max(RandomRange(LifetimeMin, LifetimeMax), 0.01f);
}

UParticleModuleLocation::UParticleModuleLocation()
{
    bSpawnModule = true;
}

// Function : Assign initial world location to spawned particle
// input : Owner, Particle, SpawnTime
// Owner : emitter instance used to read component world location
// Particle : particle receiving initial location
// SpawnTime : relative spawn time within this tick
// output : Particle Location and OldLocation are set from component location plus random local offset
void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)SpawnTime;
    const FVector LocalOffset = RandomRangeVector(StartLocationMin, StartLocationMax);
    const FVector BaseLocation = Owner ? Owner->GetComponentWorldLocation() : FVector::ZeroVector;
    Particle.Location = BaseLocation + LocalOffset;
    Particle.OldLocation = Particle.Location;
}

UParticleModuleVelocity::UParticleModuleVelocity()
{
    bSpawnModule = true;
}

// Function : Assign randomized initial velocity to spawned particle
// input : Owner, Particle, SpawnTime
// Owner : emitter instance that owns the particle
// Particle : particle receiving initial velocity
// SpawnTime : relative spawn time within this tick
// output : Particle Velocity and BaseVelocity are set between StartVelocityMin and StartVelocityMax
void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.Velocity = RandomRangeVector(StartVelocityMin, StartVelocityMax);
    Particle.BaseVelocity = Particle.Velocity;
}

UParticleModuleColor::UParticleModuleColor()
{
    bSpawnModule = true;
    bUpdateModule = true;
}

// Function : Assign initial color to spawned particle
// input : Owner, Particle, SpawnTime
// Owner : emitter instance that owns the particle
// Particle : particle receiving initial color
// SpawnTime : relative spawn time within this tick
// output : Particle Color is set to StartColor
void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.Color = StartColor;
}

// Function : Interpolate active particle color over normalized lifetime
// input : Owner, DeltaTime
// Owner : emitter instance that owns active particles
// DeltaTime : elapsed time for this simulation step
// output : Each active particle Color is lerped from StartColor to EndColor
void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount(); ++ParticleIndex)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);
        Particle.Color = FColor::Lerp(StartColor, EndColor, std::clamp(Particle.RelativeTime, 0.0f, 1.0f));
    }
}

UParticleModuleSize::UParticleModuleSize()
{
    bSpawnModule = true;
    bUpdateModule = true;
}

// Function : Assign initial size to spawned particle
// input : Owner, Particle, SpawnTime
// Owner : emitter instance that owns the particle
// Particle : particle receiving initial size
// SpawnTime : relative spawn time within this tick
// output : Particle Size is set to StartSize
void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.Size = StartSize;
}

// Function : Interpolate active particle size over normalized lifetime
// input : Owner, DeltaTime
// Owner : emitter instance that owns active particles
// DeltaTime : elapsed time for this simulation step
// output : Each active particle Size is lerped from StartSize to EndSize
void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount(); ++ParticleIndex)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);
        Particle.Size = FVector::Lerp(StartSize, EndSize, std::clamp(Particle.RelativeTime, 0.0f, 1.0f));
    }
}

UParticleModuleCollision::UParticleModuleCollision()
{
    bUpdateModule = true;
}

// Function : Resolve simple plane collision for active particles
// input : Owner, DeltaTime
// Owner : emitter instance that owns active particles and component event queue
// DeltaTime : elapsed time for this simulation step
// output : Colliding particles bounce or die, collision count updates, and optional collision events are queued
void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    if (!Owner)
    {
        return;
    }

    for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount();)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);
        const bool bCrossedPlane = Particle.OldLocation.Z > CollisionPlaneZ && Particle.Location.Z <= CollisionPlaneZ;
        if (!bCrossedPlane)
        {
            ++ParticleIndex;
            continue;
        }

        Particle.Location.Z = CollisionPlaneZ;
        Particle.Velocity.Z = std::abs(Particle.Velocity.Z) * Restitution;
        ++Particle.CollisionCount;

        if (bGenerateCollisionEvents)
        {
            FParticleEventCollideData Event;
            Event.Component = Owner->GetOwningComponent();
            Event.EmitterInstance = Owner;
            Event.EmitterIndex = Owner->GetEmitterIndex();
            Event.ParticleId = Particle.ParticleId;
            Event.Location = Particle.Location;
            Event.OldLocation = Particle.OldLocation;
            Event.Velocity = Particle.Velocity;
            Event.Normal = FVector::UpVector;
            Event.Time = Particle.RelativeTime;
            Event.Hit.bHit = true;
            Event.Hit.Location = Particle.Location;
            Event.Hit.Normal = FVector::UpVector;
            Owner->QueueCollisionEvent(Event);
        }

        if (bKillOnCollision)
        {
            Owner->KillParticle(ParticleIndex);
            continue;
        }

        ++ParticleIndex;
    }
}

UParticleModuleEventGenerator::UParticleModuleEventGenerator()
{
    bUpdateModule = true;
}

// Function : Dispatch particle events queued on owning component
// input : Owner, DeltaTime
// Owner : emitter instance used to access the owning component
// DeltaTime : elapsed time for this simulation step
// output : Queued particle events on the component are broadcast and cleared
void UParticleModuleEventGenerator::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    if (Owner)
    {
        Owner->DispatchQueuedParticleEvents();
    }
}
