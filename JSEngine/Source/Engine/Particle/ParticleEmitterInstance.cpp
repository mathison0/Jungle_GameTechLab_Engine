#include "Particle/ParticleEmitterInstance.h"

#include <algorithm>

#include "Particle/ParticleModuleTypeData.h"
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
		MaxActiveParticles = std::max(SpriteTemplate->GetMaxActiveParticleCount(), 1);
		CurrentLODLevelIndex = SpriteTemplate->SelectLODLevel(0.0f);
		CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
	}
	else
	{
		ParticleSize = sizeof(FBaseParticle);
		MaxActiveParticles = 1;
	}

	// Cycle 10d: payload-aware stride가 container 내부에서 일관 적용되도록
	// PayloadBytes 계산을 Allocate 호출 앞으로 이동. silent bug ξ 해소.
	// USpriteTypeData::RequiredPayloadBytes() = 0 → Sprite 회귀 0.
	const int32 PayloadBytes = (CurrentLODLevel && CurrentLODLevel->GetTypeDataModule())
		? CurrentLODLevel->GetTypeDataModule()->RequiredPayloadBytes()
		: 0;
	InstancePayloadSize = PayloadBytes;
	PayloadOffset = ParticleSize;

	// Cycle 10d: stride source-of-truth = container. Allocate가 (ParticleSize + PayloadBytes)를
	// 받아 align 후 멤버 ParticleStride에 저장하고 단일 블록을 할당한다.
	// 이전 cycle의 redundant `new uint8/uint16` 라인은 silent bug ν 원인이므로 제거됨.
	if (!ParticleStorage.Allocate(MaxActiveParticles, ParticleSize + PayloadBytes))
	{
		MaxActiveParticles = 0;
		return;
	}

	// Allocate는 메모리 placement만 수행 — ParticleIndices 값 초기화 루프 유지 필수
	// (제거 시 첫 Spawn에서 garbage 슬롯 참조 → 즉시 crash).
	for (int32 Index = 0; Index < MaxActiveParticles; ++Index)
	{
		ParticleStorage.ParticleIndices[Index] = static_cast<uint16>(Index);
	}
}

// Function : Release particle instance memory and reset runtime state
// input : None
// output : Particle buffers are released and instance counters return to the default state
void FParticleEmitterInstance::Reset()
{
	ParticleStorage.Reset();
	delete[] InstanceData;
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
void FParticleEmitterInstance::Tick(float DeltaTime, bool bAllowSpawning)
{
	if (!SpriteTemplate || !Component || !ParticleStorage.ParticleData || !ParticleStorage.ParticleIndices || DeltaTime <= 0.0f)
	{
		return;
	}

	SelectLODLevel(Component->ComputeEmitterLODDistance());
	if (!CurrentLODLevel || !CurrentLODLevel->IsEnabled())
	{
		return;
	}

	int32 SpawnCount = 0;
	if (bAllowSpawning)
	{
		if (UParticleModuleSpawn* SpawnModule = CurrentLODLevel->GetSpawnModule())
		{
			SpawnCount = SpawnModule->ComputeSpawnCount(this, DeltaTime);
		}
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
		const uint16 SlotIndex = ParticleStorage.ParticleIndices[ActiveIndex];
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleStorage.ParticleData + SlotIndex * ParticleStorage.GetStride());
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
	std::swap(ParticleStorage.ParticleIndices[Index], ParticleStorage.ParticleIndices[LastActiveIndex]);
	--ActiveParticles;
}

FParticleEmitterRuntimeView FParticleEmitterInstance::GetRuntimeView() const
{
    FParticleEmitterRuntimeView RuntimeView;
    RuntimeView.ParticleData = ParticleStorage.ParticleData;
    RuntimeView.ParticleIndices = ParticleStorage.ParticleIndices;
    RuntimeView.ActiveParticles = ActiveParticles;
    RuntimeView.MaxActiveParticles = MaxActiveParticles;
    RuntimeView.ParticleStride = ParticleStorage.GetStride();  // Cycle 10d: container 위임
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
	if (!ParticleStorage.ParticleData || !ParticleStorage.ParticleIndices || ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<FBaseParticle*>(ParticleStorage.ParticleData + ParticleStorage.ParticleIndices[ActiveIndex] * ParticleStorage.GetStride());
}

// Function : Get read-only particle data by active index
// input : ActiveIndex
// ActiveIndex : active particle index in the compact active list
// output : Const pointer to particle data, or nullptr when the index is invalid
const FBaseParticle* FParticleEmitterInstance::GetParticle(int32 ActiveIndex) const
{
	if (!ParticleStorage.ParticleData || !ParticleStorage.ParticleIndices || ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<const FBaseParticle*>(ParticleStorage.ParticleData + ParticleStorage.ParticleIndices[ActiveIndex] * ParticleStorage.GetStride());
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

// Function : Build Sprite instance data into internal buffer (base/Sprite path — Cycle 10c)
// input : None
// output : SpriteInstanceDataBuffer 새로 채움 — RenderCommand 매핑은 Builder가 별도 수행 (계층 분리)
// Mesh/Ribbon/Beam derived instance가 override해 자기 type의 buffer를 채움 (Cycle 11+).
void FParticleEmitterInstance::BuildInstanceData()
{
    SpriteInstanceDataBuffer.clear();
    if (ActiveParticles <= 0)
    {
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
}

// Function : Expose Sprite instance buffer to Builder (base/Sprite path)
// input : OutCount (out-param)
// output : Pointer to first element + count, or nullptr/0 when empty
// USpriteTypeData가 base instance 사용 (Cycle 8/9 결정) — base가 직접 Sprite buffer 노출.
const FSpriteParticleInstanceData* FParticleEmitterInstance::GetSpriteInstanceData(uint32& OutCount) const
{
    OutCount = static_cast<uint32>(SpriteInstanceDataBuffer.size());
    return SpriteInstanceDataBuffer.empty() ? nullptr : SpriteInstanceDataBuffer.data();
}

// Function : Mesh instance data getter — base default returns nullptr
// input : OutCount (out-param, always set to 0)
// output : Always nullptr — Mesh derived instance가 Cycle 11에서 override해 자기 buffer 노출
const FMeshParticleInstanceData* FParticleEmitterInstance::GetMeshInstanceData(uint32& OutCount) const
{
    OutCount = 0;
    return nullptr;
}

// Function : Ribbon vertex data getter — base default returns nullptr
// input : OutCount (out-param, always set to 0)
// output : Always nullptr — Ribbon derived instance가 Cycle 12+에서 override
const FRibbonParticleVertex* FParticleEmitterInstance::GetRibbonVertexData(uint32& OutCount) const
{
    OutCount = 0;
    return nullptr;
}

// Function : Beam vertex data getter — base default returns nullptr
// input : OutCount (out-param, always set to 0)
// output : Always nullptr — Beam derived instance가 Cycle 13+에서 override
const FBeamParticleVertex* FParticleEmitterInstance::GetBeamVertexData(uint32& OutCount) const
{
    OutCount = 0;
    return nullptr;
}
