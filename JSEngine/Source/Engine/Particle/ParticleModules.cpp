#include "Particle/ParticleModules.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Core/Random/EngineRandom.h"
#include "Core/ResourceManager.h"
#include "Core/ResourceTypes.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
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

static float RandomUnitFromSeed(uint32 Seed)
{
    Seed ^= Seed >> 16;
    Seed *= 0x7feb352du;
    Seed ^= Seed >> 15;
    Seed *= 0x846ca68bu;
    Seed ^= Seed >> 16;
    return static_cast<float>(Seed & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

static FVector RandomRangeVectorSeeded(const FVector& Min, const FVector& Max, uint32 Seed)
{
    return FVector(
        Min.X + (Max.X - Min.X) * RandomUnitFromSeed(Seed ^ 0x9e3779b9u),
        Min.Y + (Max.Y - Min.Y) * RandomUnitFromSeed(Seed ^ 0x85ebca6bu),
        Min.Z + (Max.Z - Min.Z) * RandomUnitFromSeed(Seed ^ 0xc2b2ae35u));
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

void UParticleModuleRequired::PostEditProperty(const char* PropertyName)
{
    UParticleModule::PostEditProperty(PropertyName);
    (void)PropertyName;
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
    const float EvaluatedRate = EvaluateFloatDistribution("Rate", Rate, Rate, 0.0f);
    if (!Owner || EvaluatedRate <= 0.0f || DeltaTime <= 0.0f)
    {
        return 0;
    }

	return Owner->ConsumeSpawnCount(EvaluatedRate, DeltaTime);
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
    Particle.Lifetime = std::max(EvaluateFloatDistribution("LifetimeMin", LifetimeMin, LifetimeMax, std::clamp(SpawnTime, 0.0f, 1.0f)), 0.01f);
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
    const FVector LocalOffset = EvaluateVectorDistribution("StartLocationMin", StartLocationMin, StartLocationMax, std::clamp(SpawnTime, 0.0f, 1.0f));
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
    Particle.Velocity = EvaluateVectorDistribution("StartVelocityMin", StartVelocityMin, StartVelocityMax, std::clamp(SpawnTime, 0.0f, 1.0f));
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
// output : Particle Size is randomized between StartSizeMin and StartSizeMax
void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    Particle.Size = EvaluateVectorDistribution("StartSizeMin", StartSizeMin, StartSizeMax, std::clamp(SpawnTime, 0.0f, 1.0f));
}

// Function : Interpolate active particle size over normalized lifetime
// input : Owner, DeltaTime
// Owner : emitter instance that owns active particles
// DeltaTime : elapsed time for this simulation step
// output : Each active particle Size moves toward a randomized end size range
void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount(); ++ParticleIndex)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);
        const FVector EndSize = EvaluateVectorDistribution("EndSizeMin", EndSizeMin, EndSizeMax, std::clamp(Particle.RelativeTime, 0.0f, 1.0f));
        Particle.Size = FVector::Lerp(Particle.Size, EndSize, std::clamp(Particle.RelativeTime, 0.0f, 1.0f));
    }
}

UParticleModuleCollision::UParticleModuleCollision()
{
    bUpdateModule = true;
}

// Function : Resolve world collision for active particles
// input : Owner, DeltaTime
// Owner : emitter instance that owns active particles and component event queue
// DeltaTime : elapsed time for this simulation step
// output : Colliding particles apply response, collision count updates, and optional collision events are queued
void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    if (!bCollisionEnabled || !Owner)
    {
        return;
    }

    UParticleSystemComponent* Component = Owner->GetOwningComponent();
    AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;
    UWorld* World = OwnerActor ? OwnerActor->GetFocusedWorld() : nullptr;
    if (!Component || !World)
    {
        return;
    }

    if (MaxCollisionDistance > 0.0f && Component->ComputeEmitterLODDistance() > MaxCollisionDistance)
    {
        return;
    }

    const float ClampedCheckFraction = std::clamp(CollisionCheckFraction, 0.0f, 1.0f);
    const float ClampedRestitution = std::clamp(Restitution, 0.0f, 1.0f);
    const float ClampedFriction = std::clamp(Friction, 0.0f, 1.0f);

    for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount();)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);

        if (ClampedCheckFraction <= 0.0f)
        {
            ++ParticleIndex;
            continue;
        }
        if (ClampedCheckFraction < 1.0f)
        {
            constexpr uint32 BucketCount = 100;
            const uint32 Threshold = static_cast<uint32>(ClampedCheckFraction * static_cast<float>(BucketCount));
            if ((Particle.ParticleId % BucketCount) >= Threshold)
            {
                ++ParticleIndex;
                continue;
            }
        }

        if (MaxCollisions > 0 && Particle.CollisionCount >= MaxCollisions)
        {
            if (bKillWhenMaxCollisionsReached)
            {
                Owner->KillParticle(ParticleIndex);
                continue;
            }
            ++ParticleIndex;
            continue;
        }

        FHitResult Hit;
        FCollisionQueryParams QueryParams;
        QueryParams.IgnoredActor = bIgnoreOwner ? OwnerActor : nullptr;
        QueryParams.IgnoredComponent = Component;

        bool bHit = false;
        if (TraceMode == EParticleCollisionTraceMode::Sphere)
        {
            float SweepRadius = std::max(0.0f, CollisionRadius);
            if (bUseParticleSizeAsRadius)
            {
                SweepRadius = std::max({
                    std::fabs(Particle.Size.X),
                    std::fabs(Particle.Size.Y),
                    std::fabs(Particle.Size.Z) }) * 0.5f;
            }

            if (SweepRadius > 0.0f)
            {
                bHit = World->SweepSingle(
                    Hit,
                    Particle.OldLocation,
                    Particle.Location,
                    FQuat::Identity,
                    FCollisionShape::MakeSphere(SweepRadius),
                    QueryParams);
            }
            else
            {
                bHit = World->LineTraceSingle(Particle.OldLocation, Particle.Location, Hit, QueryParams);
            }
        }
        else
        {
            bHit = World->LineTraceSingle(Particle.OldLocation, Particle.Location, Hit, QueryParams);
        }

        if (!bHit)
        {
            ++ParticleIndex;
            continue;
        }

        ++Particle.CollisionCount;

        EParticleCollisionResponse AppliedResponse = Response;
        if (MaxCollisions > 0 && Particle.CollisionCount >= MaxCollisions && bKillWhenMaxCollisionsReached)
        {
            AppliedResponse = EParticleCollisionResponse::Kill;
        }

        FVector HitNormal = Hit.Normal.GetSafeNormal().IsNearlyZero()
            ? FVector::UpVector
            : Hit.Normal.GetSafeNormal();

        constexpr float ParticleCollisionSkin = 0.01f;

        switch (AppliedResponse)
        {
        case EParticleCollisionResponse::Bounce:
        {
            const FVector Velocity = Particle.Velocity;
            const float NormalSpeed = FVector::DotProduct(Velocity, HitNormal);
            const FVector NormalVelocity = HitNormal * NormalSpeed;
            const FVector TangentVelocity = Velocity - NormalVelocity;

            if (NormalSpeed < 0.0f)
            {
                Particle.Velocity = TangentVelocity * (1.0f - ClampedFriction) - NormalVelocity * ClampedRestitution;
            }
            Particle.Location = Hit.Location + HitNormal * ParticleCollisionSkin;
            break;
        }
        case EParticleCollisionResponse::Stop:
            Particle.Location = Hit.Location + HitNormal * ParticleCollisionSkin;
            Particle.Velocity = FVector::ZeroVector;
            break;
        case EParticleCollisionResponse::Ignore:
            break;
        case EParticleCollisionResponse::Kill:
            Particle.Location = Hit.Location;
            break;
        }

        if (bGenerateCollisionEvents)
        {
            FParticleEventCollideData Event;
            Event.Component = Component;
            Event.EmitterInstance = Owner;
            Event.EmitterIndex = Owner->GetEmitterIndex();
            Event.ParticleId = Particle.ParticleId;
            Event.Location = Particle.Location;
            Event.OldLocation = Particle.OldLocation;
            Event.Velocity = Particle.Velocity;
            Event.Normal = HitNormal;
            Event.HitComponent = Hit.HitComponent;
            Event.HitActor = Hit.HitComponent ? Hit.HitComponent->GetOwner() : nullptr;
            Event.Time = Particle.RelativeTime;
            Event.Hit = Hit;
            Owner->QueueCollisionEvent(Event);
        }

        if (AppliedResponse == EParticleCollisionResponse::Kill)
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
    if (Owner && bDispatchCollisionEvents)
    {
        Owner->DispatchQueuedParticleEvents();
    }
}

USubUVModule::USubUVModule()
{
    bSpawnModule = true;
    bUpdateModule = true;
    SetSubUVName(FName::None);
}

void USubUVModule::Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime)
{
    (void)Owner;
    (void)SpawnTime;
    Particle.SubUVIndex = static_cast<uint32>(GetStartFrameIndex());
}

void USubUVModule::Update(FParticleEmitterInstance* Owner, float DeltaTime)
{
    (void)DeltaTime;
    if (!Owner)
    {
        return;
    }

    uint32 TotalFrames = 0;
    if (CachedSubUV)
    {
        TotalFrames = CachedSubUV->Columns * CachedSubUV->Rows;
    }
    if (TotalFrames == 0)
    {
        const UParticleLODLevel* LODLevel = Owner->GetCurrentLODLevel();
        const UParticleModuleRequired* RequiredModule = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
        if (RequiredModule)
        {
            TotalFrames = static_cast<uint32>(
                std::max(RequiredModule->GetSubImagesHorizontal(), 1) *
                std::max(RequiredModule->GetSubImagesVertical(), 1));
        }
    }
    if (TotalFrames == 0)
    {
        return;
    }

    const uint32 LastFrame = TotalFrames - 1;
    const uint32 StartFrame = std::min(static_cast<uint32>(GetStartFrameIndex()), LastFrame);
    const uint32 EndFrame = (EndFrameIndex <= 0)
        ? LastFrame
        : std::min(static_cast<uint32>(GetEndFrameIndex()), LastFrame);
    const uint32 RangeStart = std::min(StartFrame, EndFrame);
    const uint32 RangeEnd = std::max(StartFrame, EndFrame);
    const uint32 RangeFrameCount = RangeEnd - RangeStart + 1;

    for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount(); ++ParticleIndex)
    {
        FBaseParticle& Particle = *Owner->GetParticle(ParticleIndex);
        const float Clamped = std::clamp(Particle.RelativeTime, 0.0f, 0.9999f);
        const uint32 RangeFrameOffset = static_cast<uint32>(Clamped * static_cast<float>(RangeFrameCount)) % RangeFrameCount;
        Particle.SubUVIndex = RangeStart + RangeFrameOffset;
    }
}

void USubUVModule::Serialize(FArchive& Ar)
{
    UParticleModule::Serialize(Ar);
    if (Ar.IsLoading())
    {
        SetSubUVName(SubUVName);
    }
}

void USubUVModule::PostEditProperty(const char* PropertyName)
{
    UParticleModule::PostEditProperty(PropertyName);
    if (PropertyName && strcmp(PropertyName, "SubUVName") == 0)
    {
        SetSubUVName(SubUVName);
    }
}

void USubUVModule::SetSubUVName(const FName& InName)
{
    SubUVName = InName;
    CachedSubUV = FResourceManager::Get().FindSubUVExact(InName);
    if (CachedSubUV && EndFrameIndex <= 0)
    {
        const uint32 TotalFrames = CachedSubUV->Columns * CachedSubUV->Rows;
        if (TotalFrames > 0)
        {
            EndFrameIndex = static_cast<int32>(TotalFrames - 1);
        }
    }
}
