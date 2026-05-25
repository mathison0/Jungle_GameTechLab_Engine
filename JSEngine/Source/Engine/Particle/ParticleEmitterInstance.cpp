#include "Particle/ParticleEmitterInstance.h"

#include <algorithm>

#include "Particle/ParticleModuleTypeData.h"
#include "Particle/ParticleSystemComponent.h"
#include "Render/Scene/RenderCommand.h"

// Cycle 10b baseline: SpriteInstanceDataBuffer (TArray<FSpriteParticleInstanceData>) 멤버 추가.
// Cycle 9 baseline 96 → Cycle 10b 128 (+32 bytes: TArray 멤버 + 정렬 padding).
// TArray가 std::vector 기반(포인터 3개=24) + 8-byte alignment padding으로 +32 측정.
static_assert(sizeof(FParticleEmitterInstance) == 128, "Cycle 10b baseline: FParticleEmitterInstance expected 128 bytes after SpriteInstanceDataBuffer addition (Cycle 9 baseline 96 + TArray 32)");

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
		MaxActiveParticles = std::max(SpriteTemplate->GetMaxActiveParticleCount(), 1);
		CurrentLODLevelIndex = SpriteTemplate->SelectLODLevel(0.0f);
		CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
	}
	else
	{
		ParticleSize = sizeof(FBaseParticle);
		MaxActiveParticles = 1;
	}

	// payload-aware stride: TypeData가 요구하는 추가 byte 만큼 가산.
	// USpriteTypeData::RequiredPayloadBytes() = 0 → Sprite는 회귀 0.
	const int32 PayloadBytes = (CurrentLODLevel && CurrentLODLevel->GetTypeDataModule())
		? CurrentLODLevel->GetTypeDataModule()->RequiredPayloadBytes()
		: 0;
	ParticleStride = ParticleSize + PayloadBytes;
	InstancePayloadSize = PayloadBytes;
	PayloadOffset = ParticleSize;

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

FParticleEmitterRuntimeView FParticleEmitterInstance::GetRuntimeView() const
{
    FParticleEmitterRuntimeView RuntimeView;
    RuntimeView.ParticleData = ParticleData;
    RuntimeView.ParticleIndices = ParticleIndices;
    RuntimeView.ActiveParticles = ActiveParticles;
    RuntimeView.MaxActiveParticles = MaxActiveParticles;
    RuntimeView.ParticleStride = ParticleStride;
    RuntimeView.ParticleSize = ParticleSize;
    RuntimeView.CurrentLODLevelIndex = CurrentLODLevelIndex;

	if (CurrentLODLevel)
    {
        // TypeDataModule을 single source로, 없을 때 RequiredModule.RenderMode로 fallback.
        RuntimeView.RenderMode = CurrentLODLevel->GetEffectiveRenderMode();
    }

    return RuntimeView;
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


FVector FParticleEmitterInstance::GetComponentWorldLocation() const
{
    if (Component)
        return Component->GetWorldLocation();

    return FVector::ZeroVector;
}

void FParticleEmitterInstance::QueueCollisionEvent(const FParticleEventCollideData& EventData)
{
    if (Component)
        Component->QueueCollisionEvent(EventData);
}

void FParticleEmitterInstance::DispatchQueuedParticleEvents()
{
    if (Component)
        Component->DispatchQueuedParticleEvents();
}

int32 FParticleEmitterInstance::ConsumeSpawnCount(float Rate, float DeltaTime)
{
    if (Rate <= 0.0f || DeltaTime <= 0.0f)
    {
        return 0;
    }

    const float SpawnAmount = Rate * DeltaTime + SpawnFraction;
    const int32 SpawnCount = static_cast<int32>(std::floor(SpawnAmount));
    SpawnFraction = SpawnAmount - static_cast<float>(SpawnCount);
    return SpawnCount;
}

// Function : Query payload byte requirement from current LOD's TypeDataModule
// input : None
// output : Bytes required by TypeData beyond FBaseParticle, or 0 when absent
int32 FParticleEmitterInstance::GetRequiredPayloadBytes() const
{
    if (CurrentLODLevel)
    {
        if (const UParticleModuleTypeDataBase* TypeData = CurrentLODLevel->GetTypeDataModule())
        {
            return TypeData->RequiredPayloadBytes();
        }
    }
    return 0;
}

// Function : Build Sprite instance data into OutCmd (base/Sprite path — Cycle 10b)
// input : OutCmd
// OutCmd : render command whose type-specific slots get filled
// output : SpriteInstanceDataBuffer 새로 채우고 OutCmd.VertexFactoryType=SpriteParticle + ParticleInstances/Count 설정
// 이전 cycle 위치: UParticleSystemComponent::BuildSpriteInstanceData의 emitter 1개 처리 루프 — 본 메서드로 이전.
// Mesh/Ribbon/Beam derived instance가 override해 자기 type의 슬롯을 채움 (Cycle 11+).
void FParticleEmitterInstance::BuildInstanceData(FRenderCommand& OutCmd)
{
    SpriteInstanceDataBuffer.clear();
    if (ActiveParticles <= 0)
    {
        // 빈 결과라도 type/slot은 셋업 — RenderPass가 nullptr/count==0 가드로 자연 skip.
        OutCmd.VertexFactoryType = EVertexFactoryType::SpriteParticle;
        OutCmd.ParticleInstances = nullptr;
        OutCmd.ParticleInstanceCount = 0;
        return;
    }

    SpriteInstanceDataBuffer.reserve(ActiveParticles);
    for (int32 i = 0; i < ActiveParticles; ++i)
    {
        const FBaseParticle* Particle = GetParticle(i);
        if (!Particle)
        {
            continue;
        }

        FSpriteParticleInstanceData Data;
        Data.Position   = Particle->Location;
        Data.Size       = FVector2(Particle->Size.X, Particle->Size.Y);
        Data.Color      = Particle->Color;
        Data.Rotation   = Particle->Rotation;
        Data.SubUVIndex = Particle->SubUVIndex;
        SpriteInstanceDataBuffer.push_back(Data);
    }

    OutCmd.VertexFactoryType = EVertexFactoryType::SpriteParticle;
    OutCmd.ParticleInstances = SpriteInstanceDataBuffer.data();
    OutCmd.ParticleInstanceCount = static_cast<uint32>(SpriteInstanceDataBuffer.size());
}
