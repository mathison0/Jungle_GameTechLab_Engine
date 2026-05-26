#pragma once

#include "Particle/ParticleModuleTypeData.h"
#include "Particle/ParticleRibbonTypes.h"
#include "Render/Resource/Material.h"

// Ribbon emitter용 TypeData (Cycle 12, 결정 6 옵션 A + 결정 8 옵션 A).
// RequiredPayloadBytes()가 sizeof(FRibbonParticlePayload)를 반환 — container Stride에 자동 가산.
// CreateInstance()가 FParticleRibbonEmitterInstance를 반환해 SpawnParticles/KillParticle/Tick override가 작동한다.
//
// 결정 9 옵션 B: bRenderGeometry/SpawnPoints/Tangents 디버그 플래그는 본 cycle 제외 — 후속 cycle (12c) 에서 추가.
UCLASS()
class URibbonTypeData : public UParticleModuleTypeDataBase
{
public:
    GENERATED_BODY(URibbonTypeData, UParticleModuleTypeDataBase)

    int32 RequiredPayloadBytes() const override { return sizeof(FRibbonParticlePayload); }
    EParticleEmitterRenderMode GetRenderMode() const override { return EParticleEmitterRenderMode::Ribbon; }
    FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const override;

    int32 GetMaxTrailCount() const { return MaxTrailCount; }
    int32 GetMaxParticleInTrailCount() const { return MaxParticleInTrailCount; }
    float GetSheetsPerTrail() const { return SheetsPerTrail; }
    float GetTangentSpawningScalar() const { return TangentSpawningScalar; }
    UMaterialInterface* GetMaterial() const { return Material; }

    // Detail panel 의 picker 가 호출 — Material 만 변경. 다른 멤버는 reflection 으로 자동 노출됨.
    void SetMaterial(UMaterialInterface* InMaterial) { Material = InMaterial; }
    void SetMaxTrailCount(int32 InCount) { MaxTrailCount = InCount; }
    void SetMaxParticleInTrailCount(int32 InCount) { MaxParticleInTrailCount = InCount; }
    void SetSheetsPerTrail(float InValue) { SheetsPerTrail = InValue; }
    void SetTangentSpawningScalar(float InValue) { TangentSpawningScalar = InValue; }

private:
    UPROPERTY(DisplayName = "Max Trail Count", Category = "Ribbon")
    int32 MaxTrailCount = 1;

    UPROPERTY(DisplayName = "Max Particle In Trail", Category = "Ribbon")
    int32 MaxParticleInTrailCount = 64;

    UPROPERTY(DisplayName = "Sheets Per Trail", Category = "Ribbon")
    float SheetsPerTrail = 1.0f;

    UPROPERTY(DisplayName = "Tangent Spawning Scalar", Category = "Ribbon")
    float TangentSpawningScalar = 0.0f;

    UPROPERTY(DisplayName = "Material", Category = "Ribbon", ReferenceKind = Asset)
    UMaterialInterface* Material = nullptr;
};
