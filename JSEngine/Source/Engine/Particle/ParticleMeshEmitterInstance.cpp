#include "Particle/ParticleMeshEmitterInstance.h"

#include <algorithm>
#include <cmath>

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Particle/ParticleMeshTypes.h"
#include "Particle/ParticleModuleTypeDataMesh.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemComponent.h"

namespace
{
    // Cycle 14: shader 와 일관된 small-number / parallel 임계값.
    constexpr float MeshAlignSmallNumber = 1.0e-6f;
    constexpr float MeshAlignParallelDot = 0.99f;

    // Cycle 14 (M1+M2): shader MeshParticle.hlsl:37-57 (EulerZYXToMatrix) 를 CPU 측 mirror.
    // **DirectX left-handed positive rotation** — `FMatrix::MakeRotationX/Y/Z` 와 sign 반대.
    // 따라서 FMatrix::MakeRotationEuler 를 재사용하면 shader 와 다른 결과 (회전 방향 반대) →
    // 본 cycle 은 별도 local helper 로 shader 와 정확히 일치시킴.
    //
    // 매트릭스 형태 (row 0..2, col 0..2) — `Rx * Ry * Rz` 전개 결과:
    //   |  cy*cz,                -cy*sz,                sy    |
    //   |  sx*sy*cz + cx*sz,    -sx*sy*sz + cx*cz,    -sx*cy |
    //   | -cx*sy*cz + sx*sz,     cx*sy*sz + sx*cz,     cx*cy |
    //
    // 적용 방식 (shader): `v_world = v_local * MeshEuler(InstanceRotation)` (row vector × matrix).
    FMatrix MakeShaderEulerRotation(const FVector& EulerRad)
    {
        const float sx = std::sin(EulerRad.X), cx = std::cos(EulerRad.X);
        const float sy = std::sin(EulerRad.Y), cy = std::cos(EulerRad.Y);
        const float sz = std::sin(EulerRad.Z), cz = std::cos(EulerRad.Z);
        return FMatrix(
             cy * cz,                 -cy * sz,                  sy,      0.0f,
             sx * sy * cz + cx * sz,  -sx * sy * sz + cx * cz,  -sx * cy, 0.0f,
            -cx * sy * cz + sx * sz,   cx * sy * sz + sx * cz,   cx * cy, 0.0f,
             0.0f,                     0.0f,                     0.0f,    1.0f);
    }

    // Cycle 14: MakeShaderEulerRotation 의 inverse.
    // 입력 matrix 의 회전 성분에서 (x, y, z) Euler radians 를 추출.
    //   sin(y) = M[0][2]
    //   tan(z) = -M[0][1] / M[0][0]
    //   tan(x) = -M[1][2] / M[2][2]
    // Gimbal lock (cy ≈ 0): Z 를 0 으로 고정, X 가 singular degree 흡수.
    // 정확성: MakeShaderEulerRotation(ExtractShaderEuler(M)) ≈ M (gimbal lock 제외).
    FVector ExtractShaderEuler(const FMatrix& M)
    {
        const float SinY = std::clamp(M.M[0][2], -1.0f, 1.0f);
        const float Y = std::asin(SinY);
        const float Cy = std::cos(Y);
        FVector Result;
        if (std::fabs(Cy) > MeshAlignSmallNumber)
        {
            Result.X = std::atan2(-M.M[1][2], M.M[2][2]);
            Result.Y = Y;
            Result.Z = std::atan2(-M.M[0][1], M.M[0][0]);
        }
        else
        {
            // Gimbal lock (sy ≈ ±1) — Z = 0 고정, X 가 singular degree 흡수.
            Result.X = std::atan2(M.M[1][0], M.M[1][1]);
            Result.Y = Y;
            Result.Z = 0.0f;
        }
        return Result;
    }

    // Cycle 14 (M1): shader row-vector convention 의 alignment matrix.
    // mesh 의 local +X 축이 Forward 방향으로 회전하도록 하는 orthonormal basis.
    // Row 0 = X 축 위치 (= Forward), Row 1 = Y (UpHint cross), Row 2 = Z (X cross Y).
    //
    // Forward zero / Forward∥UpHint singular 처리:
    //   - Forward.IsNearlyZero() → Identity 반환 (정렬 의미 없음).
    //   - |dot(Forward, UpHint)| > parallel 임계값 → UpHint 를 X 또는 Y 축으로 자동 전환
    //     (Beam 의 ComputeBeamLocalAxes 패턴 답습 — 위험 11 일반화).
    FMatrix MakeAlignmentMatrix(const FVector& Forward, const FVector& UpHint)
    {
        const FVector X = Forward.GetSafeNormal();
        if (X.IsNearlyZero())
        {
            return FMatrix::Identity;
        }
        FVector Up = UpHint.GetSafeNormal();
        if (Up.IsNearlyZero() || std::fabs(X.DotProduct(Up)) > MeshAlignParallelDot)
        {
            // parallel singular → 다른 축 fallback. X 의 dominant component 와 다른 축 선택.
            Up = (std::fabs(X.X) < MeshAlignParallelDot)
                ? FVector(1.0f, 0.0f, 0.0f)
                : FVector(0.0f, 1.0f, 0.0f);
        }
        const FVector Y = FVector::CrossProduct(Up, X).GetSafeNormal();
        const FVector Z = FVector::CrossProduct(X, Y).GetSafeNormal();
        return FMatrix(
            X.X, X.Y, X.Z, 0.0f,
            Y.X, Y.Y, Y.Z, 0.0f,
            Z.X, Z.Y, Z.Z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }
}

// Function : Lookup mesh rotation payload by physical slot index
// input : SlotIndex
// SlotIndex : physical slot in ParticleStorage.ParticleData (not active index)
// output : pointer to interleaved FMeshRotationPayload, or nullptr when storage not ready
//
// Cycle 14 (결정 20 옵션 A): private → public 승격. UParticleModuleMeshRotationRate 의 Spawn/Update 에서
//   Cast<FParticleMeshEmitterInstance>(Owner) 후 호출. base class 변경 0건 — Mesh derived 만 노출.
FMeshRotationPayload* FParticleMeshEmitterInstance::GetMeshPayload(int32 SlotIndex)
{
    if (!ParticleStorage.ParticleData || SlotIndex < 0)
    {
        return nullptr;
    }
    uint8* ParticleBase = ParticleStorage.ParticleData + SlotIndex * ParticleStorage.GetStride();
    return reinterpret_cast<FMeshRotationPayload*>(ParticleBase + PayloadOffset);
}

// Function : Lookup mesh rotation payload by active index (compact list)
// input : ActiveIdx
// ActiveIdx : index in [0, ActiveParticles) — compact active list
// output : pointer to interleaved FMeshRotationPayload, or nullptr when invalid
//
// Cycle 14 (M2): UParticleModuleMeshRotationRate::Update 의 active 순회용 편의 helper.
// ActiveIdx → SlotIndex 변환 (ParticleStorage.ParticleIndices) + payload 회수.
FMeshRotationPayload* FParticleMeshEmitterInstance::GetMeshPayloadAt(int32 ActiveIdx)
{
    if (!ParticleStorage.ParticleData || !ParticleStorage.ParticleIndices ||
        ActiveIdx < 0 || ActiveIdx >= ActiveParticles)
    {
        return nullptr;
    }
    const int32 SlotIndex = static_cast<int32>(ParticleStorage.ParticleIndices[ActiveIdx]);
    return GetMeshPayload(SlotIndex);
}

// Function : Spawn particles via base and initialize mesh rotation payload for new slots
// input : Count, StartTime, Increment, InitialLocation, InitialVelocity, EventPayload
// output : Base particles spawned + payload initialized at (InitialOrientation = 0, Rotation = 0, RotRate = 0)
//
// Cycle 11 (옵션 B): 모든 rotation 필드 0 고정 — RotRate 모듈 부재 시.
// Cycle 14 (M2): RotRate 는 UParticleModuleMeshRotationRate::Spawn 에서 사용자 입력으로 덮어쓰여짐 (Color/Size 패턴).
//   기본값 zero 초기화는 회귀 안전 (M2 module 미사용 시 Cycle 11 동작 그대로).
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

// Function : Build per-instance mesh data with alignment (M1) + accumulated rotation (M2) combination
// input : None
// output : MeshInstanceDataBuffer 새로 채움 — RenderCommand 매핑은 Builder가 별도 수행
//
// Cycle 14 (M1 + M2, 결정 21 A): "Rotate then Spin" UE Cascade 표준.
//   AccumulatedMatrix = MakeShaderEulerRotation(Payload->Rotation)  — 매 frame UParticleModuleMeshRotationRate::Update 가 누적.
//   AlignmentMatrix   = MakeAlignmentMatrix(Forward, UpHint)         — alignment 모드에 따라 산출.
//   Final             = AccumulatedMatrix * AlignmentMatrix          — row-vector convention: v * Acc * Align.
//                                                                       (Acc 가 mesh 의 local frame 에서 spin, Align 이 world 로 orient.)
//   Data.InstanceRotation = ExtractShaderEuler(Final)                — shader 가 다시 EulerZYXToMatrix 로 같은 matrix 재구성.
//
// alignment 모드 분기:
//   PSA_Velocity              — Forward = Particle->Velocity.Normalize(). zero 면 alignment Identity.
//   PSA_FacingCameraPosition  — Forward = (CachedCameraPos - Particle->Location).Normalize().
//                                bCachedCameraValid=false (예: EditorMainPanelDebug.cpp:170 호출 경로) 면
//                                PSA_Velocity 로 fallback (위험 12 방어).
//
// UpHint: 항상 world Up (0,0,1). MakeAlignmentMatrix 내부에서 parallel 시 자동 axis 전환.
void FParticleMeshEmitterInstance::BuildInstanceData()
{
    MeshInstanceDataBuffer.clear();
    if (ActiveParticles <= 0)
    {
        return;
    }

    // (1) TypeData / alignment 모드 lookup (frame 단위 1회).
    EMeshAlignment AlignmentMode = EMeshAlignment::PSA_Velocity;
    if (UParticleLODLevel* LOD = GetCurrentLODLevel())
    {
        if (const UMeshTypeData* MeshTD = Cast<UMeshTypeData>(LOD->GetTypeDataModule()))
        {
            AlignmentMode = MeshTD->GetAlignment();
        }
    }

    // (2) Component cached camera (옵션 β). bCachedCameraValid=false 면 PSA_FacingCameraPosition 은 PSA_Velocity 로 fallback.
    const UParticleSystemComponent* OwningComp = GetOwningComponent();
    const bool bCameraValid = OwningComp && OwningComp->IsCachedCameraValid();
    const FVector CameraPos = bCameraValid ? OwningComp->GetCachedCameraPosition() : FVector::ZeroVector;
    const FVector WorldUp(0.0f, 0.0f, 1.0f);

    // 위험 12 방어: PSA_FacingCameraPosition 선택됐으나 camera invalid → effective mode 를 Velocity 로.
    const EMeshAlignment EffectiveAlignment =
        (AlignmentMode == EMeshAlignment::PSA_FacingCameraPosition && !bCameraValid)
            ? EMeshAlignment::PSA_Velocity
            : AlignmentMode;

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

        // (3) Spin matrix — payload 의 Rotation (Cycle 11 옵션 B 의 3축 Euler radians).
        //     UParticleModuleMeshRotationRate (M2) 가 매 Update 에 Rotation 을 누적.
        const FVector PayloadRotation = Payload ? Payload->Rotation : FVector::ZeroVector;
        const FMatrix SpinMatrix = MakeShaderEulerRotation(PayloadRotation);

        // (4) Alignment matrix — 모드별 산출.
        FMatrix AlignmentMatrix = FMatrix::Identity;
        switch (EffectiveAlignment)
        {
        case EMeshAlignment::PSA_Velocity:
        {
            // velocity zero 면 MakeAlignmentMatrix 내부에서 Identity 반환 (위험 12 의 일반화).
            AlignmentMatrix = MakeAlignmentMatrix(Particle->Velocity, WorldUp);
            break;
        }
        case EMeshAlignment::PSA_FacingCameraPosition:
        {
            const FVector ToCamera = CameraPos - Particle->Location;
            AlignmentMatrix = MakeAlignmentMatrix(ToCamera, WorldUp);
            break;
        }
        default:
            // 신규 alignment 값 추가 시 enum 추가 + 본 switch case 추가 필요.
            // default Identity 는 silent fallback 이므로 enum 만 추가하고 case 잊으면 알지 못함 — 후속 cycle 의 메모.
            break;
        }

        // (5) 결정 21 옵션 A 결합: Final = Spin * Alignment (row-vector convention).
        //     v * Final = v * Spin * Alignment — local frame 에서 먼저 spin, 그 후 world 로 align.
        const FMatrix Final = SpinMatrix * AlignmentMatrix;

        FMeshParticleInstanceData Data;
        Data.InstancePosition = Particle->Location;
        Data.InstanceRotation = ExtractShaderEuler(Final);
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
