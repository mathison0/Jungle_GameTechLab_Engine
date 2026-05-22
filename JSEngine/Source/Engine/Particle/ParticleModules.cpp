#include "Particle/ParticleModules.h"

#include <algorithm>

#include "Core/Random/EngineRandom.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleSystemComponent.h"


static float RandomRange(float Min, float Max)
{
    return FEngineRandom::Get().RandomFloat(Min, Max);
}

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

int32 UParticleModuleSpawn::ComputeSpawnCount(FParticleEmitterInstance* Owner, float DeltaTime)
{
    if (!Owner || Rate <= 0.0f || DeltaTime <= 0.0f)
    {
        return 0;
    }

    const float SpawnAmount = Rate * DeltaTime + Owner->SpawnFraction;
    const int32 SpawnCount = static_cast<int32>(std::floor(SpawnAmount));
    Owner->SpawnFraction = SpawnAmount - static_cast<float>(SpawnCount);
    return SpawnCount;
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
    bSpawnModule = true;
}

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

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)SpawnTime;
    const FVector LocalOffset = RandomRangeVector(StartLocationMin, StartLocationMax);
    const FVector BaseLocation = Owner && Owner->Component ? Owner->Component->GetWorldLocation() : FVector::ZeroVector;
    Particle.Location = BaseLocation + LocalOffset;
    Particle.OldLocation = Particle.Location;
}

UParticleModuleVelocity::UParticleModuleVelocity()
{
    bSpawnModule = true;
}

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

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.Color = StartColor;
}

void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    for (int32 ParticleIndex = 0; ParticleIndex < Owner->ActiveParticles; ++ParticleIndex)
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

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.Size = StartSize;
}

void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    for (int32 ParticleIndex = 0; ParticleIndex < Owner->ActiveParticles; ++ParticleIndex)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);
        Particle.Size = FVector::Lerp(StartSize, EndSize, std::clamp(Particle.RelativeTime, 0.0f, 1.0f));
    }
}

UParticleModuleCollision::UParticleModuleCollision()
{
    bUpdateModule = true;
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    if (!Owner || !Owner->Component)
    {
        return;
    }

    for (int32 ParticleIndex = 0; ParticleIndex < Owner->ActiveParticles;)
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
            Event.Component = Owner->Component;
            Event.EmitterInstance = Owner;
            Event.EmitterIndex = Owner->EmitterIndex;
            Event.ParticleId = Particle.ParticleId;
            Event.Location = Particle.Location;
            Event.OldLocation = Particle.OldLocation;
            Event.Velocity = Particle.Velocity;
            Event.Normal = FVector::UpVector;
            Event.Time = Particle.RelativeTime;
            Event.Hit.bHit = true;
            Event.Hit.Location = Particle.Location;
            Event.Hit.Normal = FVector::UpVector;
            Owner->Component->QueueCollisionEvent(Event);
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

void UParticleModuleEventGenerator::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    if (Owner && Owner->Component)
    {
        Owner->Component->DispatchQueuedParticleEvents();
    }
}
