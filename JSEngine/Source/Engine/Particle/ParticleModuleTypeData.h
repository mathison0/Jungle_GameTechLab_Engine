#pragma once

#include "Particle/ParticleModule.h"
#include "Particle/ParticleTypes.h"

struct FParticleEmitterInstance;
class UParticleSystemComponent;

// TypeData base — emitter type (Sprite/Mesh/Beam/Ribbon) 분기의 single source.
// UParticleLODLevel::TypeDataModule 슬롯에 캐싱되며, LODLevel::CacheModuleLists() 에서
// Modules 배열을 순회하면서 Cast<UParticleModuleTypeDataBase>로 자동 추출된다.
UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleTypeDataBase, UParticleModule)

	// FBaseParticle 뒤에 type별로 요구하는 추가 payload byte 수.
	// 기본 0 (Sprite는 추가 payload 없음). Mesh/Ribbon/Beam이 override.
	virtual int32 RequiredPayloadBytes() const { return 0; }

	// 이 TypeData가 표현하는 emitter render mode.
	virtual EParticleEmitterRenderMode GetRenderMode() const { return EParticleEmitterRenderMode::Sprite; }

	// emitter runtime instance 생성 hook. 기본은 base FParticleEmitterInstance 반환.
	// Mesh/Ribbon/Beam은 파생 instance를 반환하도록 override.
	virtual FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const;
};

// Sprite emitter용 TypeData. RequiredPayloadBytes() = 0 보장이 회귀 안전의 핵심.
// 기본 Sprite asset도 이 TypeData를 LODLevel.Modules에 포함시켜 TypeData 시스템 안으로 편입한다.
UCLASS()
class USpriteTypeData : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(USpriteTypeData, UParticleModuleTypeDataBase)

	int32 RequiredPayloadBytes() const override { return 0; }
	EParticleEmitterRenderMode GetRenderMode() const override { return EParticleEmitterRenderMode::Sprite; }
	FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component, int32 EmitterIndex) const override;
};
