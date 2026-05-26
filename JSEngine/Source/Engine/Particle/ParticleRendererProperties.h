#pragma once

#include "Engine/Asset/StaticMesh.h"
#include "Object/Object.h"
#include "Particle/ParticleMeshTypes.h"
#include "Particle/ParticleRibbonTypes.h"
#include "Particle/ParticleTypes.h"
#include "Render/Resource/Material.h"

struct FParticleEmitterInstance;
class UParticleSystemComponent;

UCLASS()
class UParticleRendererProperties : public UObject
{
public:
    GENERATED_BODY(UParticleRendererProperties, UObject)

    virtual EParticleEmitterRenderMode GetRenderMode() const { return RenderMode; }
    void SetRenderMode(EParticleEmitterRenderMode InRenderMode) { RenderMode = InRenderMode; }

    virtual int32 RequiredPayloadBytes() const { return 0; }
    virtual FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const;

private:
    UPROPERTY(DisplayName = "Render Mode", NoEdit)
    EParticleEmitterRenderMode RenderMode = EParticleEmitterRenderMode::Sprite;
};

UCLASS()
class UParticleSpriteRendererProperties : public UParticleRendererProperties
{
public:
    GENERATED_BODY(UParticleSpriteRendererProperties, UParticleRendererProperties)

    EParticleEmitterRenderMode GetRenderMode() const override { return EParticleEmitterRenderMode::Sprite; }
    int32 RequiredPayloadBytes() const override { return 0; }
    FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const override;
};

UCLASS()
class UParticleMeshRendererProperties : public UParticleRendererProperties
{
public:
    GENERATED_BODY(UParticleMeshRendererProperties, UParticleRendererProperties)

    EParticleEmitterRenderMode GetRenderMode() const override { return EParticleEmitterRenderMode::Mesh; }
    int32 RequiredPayloadBytes() const override { return sizeof(FMeshRotationPayload); }
    FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const override;

    UStaticMesh* GetMesh() const { return Mesh; }
    void SetMesh(UStaticMesh* InMesh) { Mesh = InMesh; }

    void SetOverrideMaterial(bool bEnable, UMaterialInterface* InMaterial)
    {
        bOverrideMaterial = bEnable;
        OverrideMaterial = InMaterial;
    }

    UMaterialInterface* GetEffectiveMaterial() const;

private:
    UPROPERTY(DisplayName = "Static Mesh", Category = "Mesh", ReferenceKind = Asset)
    UStaticMesh* Mesh = nullptr;

    UPROPERTY(DisplayName = "Override Material", Category = "Mesh")
    bool bOverrideMaterial = false;

    UPROPERTY(DisplayName = "Material Override", Category = "Mesh", ReferenceKind = Asset)
    UMaterialInterface* OverrideMaterial = nullptr;
};

UCLASS()
class UParticleRibbonRendererProperties : public UParticleRendererProperties
{
public:
    GENERATED_BODY(UParticleRibbonRendererProperties, UParticleRendererProperties)

    EParticleEmitterRenderMode GetRenderMode() const override { return EParticleEmitterRenderMode::Ribbon; }
    int32 RequiredPayloadBytes() const override { return sizeof(FRibbonParticlePayload); }
    FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const override;

    int32 GetMaxTrailCount() const { return MaxTrailCount; }
    int32 GetMaxParticleInTrailCount() const { return MaxParticleInTrailCount; }
    float GetSheetsPerTrail() const { return SheetsPerTrail; }
    float GetTangentSpawningScalar() const { return TangentSpawningScalar; }
    UMaterialInterface* GetMaterial() const { return Material; }

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
