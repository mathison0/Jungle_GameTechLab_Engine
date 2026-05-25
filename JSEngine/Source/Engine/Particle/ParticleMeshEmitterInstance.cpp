#include "Particle/ParticleMeshEmitterInstance.h"

#include "Particle/ParticleMeshTypes.h"

// Function : Lookup mesh rotation payload by physical slot index
// input : SlotIndex
// SlotIndex : physical slot in ParticleStorage.ParticleData (not active index)
// output : pointer to interleaved FMeshRotationPayload, or nullptr when storage not ready
FMeshRotationPayload* FParticleMeshEmitterInstance::GetMeshPayload(int32 SlotIndex)
{
    if (!ParticleStorage.ParticleData || SlotIndex < 0)
    {
        return nullptr;
    }
    uint8* ParticleBase = ParticleStorage.ParticleData + SlotIndex * ParticleStorage.GetStride();
    return reinterpret_cast<FMeshRotationPayload*>(ParticleBase + PayloadOffset);
}

// Function : Spawn particles via base and initialize mesh rotation payload for new slots
// input : Count, StartTime, Increment, InitialLocation, InitialVelocity, EventPayload
// output : Base particles spawned + payload initialized at (InitialOrientation = 0, Rotation = 0, RotRate = 0)
//
// 옵션 B Cycle 11: 모든 rotation 필드 0 고정. 후속 cycle에서 UParticleModuleMeshRotationRate 도입 시
// Spawn module hook에서 payload 채움 (현재는 module 부재 → 0 초기화).
void FParticleMeshEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment,
                                                  const FVector& InitialLocation, const FVector& InitialVelocity,
                                                  FParticleEventInstancePayload* EventPayload)
{
    const int32 OldActiveCount = ActiveParticles;
    FParticleEmitterInstance::SpawnParticles(Count, StartTime, Increment, InitialLocation, InitialVelocity, EventPayload);

    // base가 spawn 한 신규 particle range [OldActiveCount, ActiveParticles) — payload 초기화.
    // SlotIndex는 ParticleIndices[ActiveIdx]에서 얻는다 (physical slot — swap-pop 안전).
    for (int32 ActiveIdx = OldActiveCount; ActiveIdx < ActiveParticles; ++ActiveIdx)
    {
        const int32 SlotIndex = static_cast<int32>(ParticleStorage.ParticleIndices[ActiveIdx]);
        if (FMeshRotationPayload* Payload = GetMeshPayload(SlotIndex))
        {
            Payload->InitialOrientation = FVector::ZeroVector;
            Payload->Rotation = FVector::ZeroVector;
            Payload->RotRate = FVector::ZeroVector;
        }
    }
}

// Function : Build per-instance mesh data into internal buffer (Cycle 11 옵션 B path)
// input : None
// output : MeshInstanceDataBuffer 새로 채움 — RenderCommand 매핑은 Builder가 별도 수행
//
// RotRate가 0 고정이므로 Rotation 누적은 본 cycle에서 no-op.
// 후속 cycle (RotRate 모듈 도입) 진입 시 DeltaTime 전달 경로 (base Tick → protected LastDeltaTime) 추가 필요.
void FParticleMeshEmitterInstance::BuildInstanceData()
{
    MeshInstanceDataBuffer.clear();
    if (ActiveParticles <= 0)
    {
        return;
    }

    MeshInstanceDataBuffer.reserve(ActiveParticles);
    for (int32 i = 0; i < ActiveParticles; ++i)
    {
        const FBaseParticle* Particle = GetParticle(i);
        if (!Particle)
        {
            continue;
        }
        const int32 SlotIndex = static_cast<int32>(ParticleStorage.ParticleIndices[i]);
        FMeshRotationPayload* Payload = GetMeshPayload(SlotIndex);

        FMeshParticleInstanceData Data;
        Data.InstancePosition = Particle->Location;
        Data.InstanceRotation = Payload ? Payload->Rotation : FVector::ZeroVector;
        Data.InstanceScale    = Particle->Size;
        Data.InstanceColor    = Particle->Color;
        MeshInstanceDataBuffer.push_back(Data);
    }
}

// Function : Expose mesh instance buffer to Builder (Mesh path override)
// input : OutCount (out-param)
// output : Pointer to first element + count, or nullptr/0 when empty
const FMeshParticleInstanceData* FParticleMeshEmitterInstance::GetMeshInstanceData(uint32& OutCount) const
{
    OutCount = static_cast<uint32>(MeshInstanceDataBuffer.size());
    return MeshInstanceDataBuffer.empty() ? nullptr : MeshInstanceDataBuffer.data();
}
