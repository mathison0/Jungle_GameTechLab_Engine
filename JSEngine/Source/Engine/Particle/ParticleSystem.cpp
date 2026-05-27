#include "Particle/ParticleSystem.h"

#include "Core/Paths.h"
#include "Particle/ParticleModuleTypeData.h"

#include <algorithm>

#include "Core/ResourceManager.h"
#include "Particle/ParticleModuleBeamNoise.h"
#include "Particle/ParticleModuleBeamSource.h"
#include "Particle/ParticleModuleBeamTarget.h"
#include "Particle/ParticleModuleTypeData.h"
#include "Particle/ParticleModuleTypeDataBeam.h"
#include "Particle/ParticleModuleTypeDataMesh.h"
#include "Particle/ParticleModuleTypeDataRibbon.h"
UParticleLODLevel::~UParticleLODLevel()
{
    ClearModules();
}

void UParticleLODLevel::PostDuplicate(UObject* Original)
{
    UObject::PostDuplicate(Original);

    UParticleLODLevel* SourceLOD = Cast<UParticleLODLevel>(Original);

    RequiredModule = nullptr;
    Modules.clear();
    SpawnModule = nullptr;
    SpawnModules.clear();
    UpdateModules.clear();
    TypeDataModule = nullptr;

    if (!SourceLOD)
    {
        return;
    }

    if (SourceLOD->RequiredModule)
    {
        RequiredModule = Cast<UParticleModuleRequired>(SourceLOD->RequiredModule->Duplicate());
    }

    for (UParticleModule* SourceModule : SourceLOD->Modules)
    {
        UParticleModule* DuplicatedModule = SourceModule ?
			Cast<UParticleModule>(SourceModule->Duplicate()) : nullptr;

        if (DuplicatedModule)
        {
            Modules.push_back(DuplicatedModule);
        }
    }

    CacheModuleLists();
}

UParticleModuleRequired* UParticleLODLevel::EnsureRequiredModule()
{
    if (!RequiredModule)
        RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();

	CacheModuleLists();
    return RequiredModule;
}

UParticleModuleSpawn* UParticleLODLevel::EnsureSpawnModule()
{
    for (UParticleModule* Module : Modules)
    {
        if (UParticleModuleSpawn* SpawnModule = Cast<UParticleModuleSpawn>(Module))
            return SpawnModule;
    }        
    return AddModule<UParticleModuleSpawn>();
}

void UParticleLODLevel::RemoveModule(UParticleModule* Module)
{
    if (!Module)
        return;
    for (auto It = Modules.begin(); It != Modules.end(); It++)
    {
        if (*It == Module)
        {
            Modules.erase(It);
            UObjectManager::Get().DestroyObject(Module);
            CacheModuleLists();
            return;
        }
    }

	  if (RequiredModule == Module)
    {
        UObjectManager::Get().DestroyObject(RequiredModule);
        RequiredModule = nullptr;
        CacheModuleLists();
    }
}

void UParticleLODLevel::ClearModules()
{
    if (RequiredModule)
    {
        UObjectManager::Get().DestroyObject(RequiredModule);
        RequiredModule = nullptr;
    }

	for (UParticleModule* Module : Modules)
    {
        if (Module)
            UObjectManager::Get().DestroyObject(Module);
    }
    Modules.clear();
    CacheModuleLists();
}

bool UParticleLODLevel::Validate(TArray<FString>* OutErrors) const
{
    bool bIsValid = true;
    if (!RequiredModule)
    {
        bIsValid = false;
        if (OutErrors)
            OutErrors->push_back("Particle LOD level must have a required module");
    }

	bool bHasSpawnModule = false;
    for (UParticleModule* Module : Modules)
    {
        if (!Module)
        {
            bIsValid = false;
            if (OutErrors)
                OutErrors->push_back("Particle LOD level must have a spawn modules");
            continue;
		}
        if (Cast<UParticleModuleSpawn>(Module))
            bHasSpawnModule = true;
    }
    if (!bHasSpawnModule)
    {
        bIsValid = false;
        if (OutErrors)
            OutErrors->push_back("Particle LOD level must have a spawn module.");
    }
    return bIsValid;
}

// Function : Build cached spawn and update module lists for this LOD level
// input : None
// output : SpawnModule, SpawnModules, UpdateModules, and TypeDataModule are refreshed from enabled modules
void UParticleLODLevel::CacheModuleLists()
{
	SpawnModule = nullptr;
	SpawnModules.clear();
	UpdateModules.clear();
	TypeDataModule = nullptr;

	if (RequiredModule && RequiredModule->IsEnabled())
	{
		SpawnModules.push_back(RequiredModule);
	}

	for (UParticleModule* Module : Modules)
	{
		if (!Module || !Module->IsEnabled())
		{
			continue;
		}

		// TypeData는 실행 모듈이 아니라 emitter runtime/render policy 슬롯으로만 캐싱한다.
		// USpriteTypeData도 여기로 잡혀 LODLevel.TypeDataModule에 들어간다 → 회귀 안전 핵심.
		if (UParticleModuleTypeDataBase* CandidateTypeData = Cast<UParticleModuleTypeDataBase>(Module))
		{
			TypeDataModule = CandidateTypeData;
			continue;
		}

		if (UParticleModuleSpawn* CandidateSpawn = Cast<UParticleModuleSpawn>(Module))
		{
			SpawnModule = CandidateSpawn;
			continue;
		}

		if (Module->IsSpawnModule())
		{
			SpawnModules.push_back(Module);
		}
		if (Module->IsUpdateModule())
		{
			UpdateModules.push_back(Module);
		}
	}
}

// Function : Resolve effective render mode with TypeData precedence and Required fallback
// input : None
// output : TypeDataModule->GetRenderMode() when present, RequiredModule->GetRenderMode() fallback, Sprite default
EParticleEmitterRenderMode UParticleLODLevel::GetEffectiveRenderMode() const
{
	if (TypeDataModule)
	{
		return TypeDataModule->GetRenderMode();
	}
	if (RequiredModule)
	{
		return RequiredModule->GetRenderMode();
	}
	return EParticleEmitterRenderMode::Sprite;
}

UParticleEmitter::~UParticleEmitter()
{
    ClearLODLevels();
}

void UParticleEmitter::PostDuplicate(UObject* Original)
{
    UObject::PostDuplicate(Original);

	UParticleEmitter* SourceEmitter = Cast<UParticleEmitter>(Original);
    LODLevels.clear();

	ParticleSize = sizeof(FBaseParticle);
    MaxActiveParticles = 128;

	if (!SourceEmitter)
        return;

	    for (UParticleLODLevel* SourceLOD : SourceEmitter->LODLevels)
    {
        UParticleLODLevel* DuplicatedLOD = SourceLOD?
			Cast<UParticleLODLevel>(SourceLOD->Duplicate()): nullptr;

        if (DuplicatedLOD)
            LODLevels.push_back(DuplicatedLOD);
    }

    CacheEmitterModuleInfo();

}

UParticleLODLevel* UParticleEmitter::AddLODLevel(int32 Level, float DistanceThreshold)
{
    UParticleLODLevel* NewLOD = UObjectManager::Get().CreateObject<UParticleLODLevel>();
    if (!NewLOD)
    {
        return nullptr;
    }
    NewLOD->Level = Level;
    NewLOD->bEnabled = true;
    NewLOD->DistanceThreshold = DistanceThreshold;

    LODLevels.push_back(NewLOD);
    SortLODLevelsByDistance();
    CacheEmitterModuleInfo();

    return NewLOD;
}

void UParticleEmitter::RemoveLODLevel(int32 Index)
{
    if (Index < 0 || Index >= static_cast<int32>(LODLevels.size()))
        return;
	UParticleLODLevel* RemovedLOD = LODLevels[Index];
    LODLevels.erase(LODLevels.begin()+ Index);

	if (RemovedLOD)
        UObjectManager::Get().DestroyObject(RemovedLOD);

	CacheEmitterModuleInfo();
}

void UParticleEmitter::ClearLODLevels()
{
    for (UParticleLODLevel* LODLevel : LODLevels)
    {
        if (LODLevel)
            UObjectManager::Get().DestroyObject(LODLevel);
    }
    LODLevels.clear();
    CacheEmitterModuleInfo();
}

void UParticleEmitter::SortLODLevelsByDistance()
{
    std::sort(LODLevels.begin(), LODLevels.end(),
        [](const UParticleLODLevel* A, const UParticleLODLevel* B)
        {
            if (!A)
            {
                return false;
            }
            if (!B)
            {
                return true;
            }
            return A->GetDistanceThreshold() < B->GetDistanceThreshold();
        });
}

bool UParticleEmitter::Validate(TArray<FString>* OutErrors) const
{
    bool bIsValid = true;

	if (LODLevels.empty())
    {
        bIsValid = false;
        if (OutErrors)
        {
            OutErrors->push_back("Particle emitter must have at least one LOD level.");
        }
    }
    for (int32 LODIndex = 0; LODIndex < static_cast<int32>(LODLevels.size()); LODIndex++)
    {
        const UParticleLODLevel* LODLevel = LODLevels[LODIndex];
        if (!LODLevel)
        {
            bIsValid = false;
            if (OutErrors)
                OutErrors->push_back("Particle emitter has a null LOD level.");
            continue;
        }
        if (!LODLevel->Validate(OutErrors))
        {
            bIsValid = false;
        }
    }
    return bIsValid;
}

// Function : Cache emitter particle layout and module information
// input : None
// output : LOD module caches are refreshed and MaxActiveParticles is updated from required modules
void UParticleEmitter::CacheEmitterModuleInfo()
{
	ParticleSize = sizeof(FBaseParticle);
	MaxActiveParticles = 128;

	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (!LODLevel)
		{
			continue;
		}

		LODLevel->CacheModuleLists();
		if (LODLevel->GetRequiredModule())
		{
			MaxActiveParticles = std::max(MaxActiveParticles, LODLevel->GetRequiredModule()->GetMaxParticles());
		}
	}
}

// Function : Get LOD level by index
// input : Index
// Index : requested LOD level index
// output : LOD level pointer, or nullptr when the index is out of range
UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(LODLevels.size()))
	{
		return nullptr;
	}
	return LODLevels[Index];
}

// Function : Select enabled LOD level for the given distance
// input : Distance
// Distance : distance from emitter component to active camera
// output : Matching LOD index, first enabled fallback index, or 0 when no enabled LOD exists
int32 UParticleEmitter::SelectLODLevel(float Distance) const
{
	int32 FallbackIndex = -1;
	for (int32 Index = 0; Index < static_cast<int32>(LODLevels.size()); ++Index)
	{
		const UParticleLODLevel* LODLevel = LODLevels[Index];
		if (!LODLevel || !LODLevel->IsEnabled())
		{
			continue;
		}

		if (FallbackIndex < 0)
		{
			FallbackIndex = Index;
		}

		if (Distance <= LODLevel->GetDistanceThreshold())
		{
			return Index;
		}
	}

	return FallbackIndex >= 0 ? FallbackIndex : 0;
}

UParticleSystem::~UParticleSystem()
{
    ClearEmitters();
}

void UParticleSystem::PostDuplicate(UObject* Original)
{
    UObject::PostDuplicate(Original);

	UParticleSystem* SourceSystem = Cast<UParticleSystem>(Original);
    Emitters.clear();

	if (!SourceSystem)
        return;

	for (UParticleEmitter* SourceEmitter : SourceSystem->Emitters)
    {
        UParticleEmitter* DuplicatedEmitter = SourceEmitter ? 
			Cast<UParticleEmitter>(SourceEmitter->Duplicate()) : nullptr;
        if (DuplicatedEmitter)
            Emitters.push_back(DuplicatedEmitter);
    }
    CacheEmitterModuleInfo();
}

UParticleEmitter* UParticleSystem::AddEmitter()
{
    UParticleEmitter* NewEmitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
    if (!NewEmitter)
    {
        return nullptr;
    }

    Emitters.push_back(NewEmitter);
    return NewEmitter;
}
void UParticleSystem::RemoveEmitter(int32 Index)
{
    if (Index < 0 || Index >= static_cast<int32>(Emitters.size()))
    {
        return;
    }

    UParticleEmitter* RemovedEmitter = Emitters[Index];
    Emitters.erase(Emitters.begin() + Index);
    if (RemovedEmitter)
        UObjectManager::Get().DestroyObject(RemovedEmitter);
}

void UParticleSystem::ClearEmitters()
{
    for (UParticleEmitter* Emitter : Emitters)
        if (Emitter)
            UObjectManager::Get().DestroyObject(Emitter);

    Emitters.clear();
}

void UParticleSystem::CacheEmitterModuleInfo()
{
    for (UParticleEmitter* Emitter : Emitters)
        if (Emitter)
            Emitter->CacheEmitterModuleInfo();
}

void UParticleSystem::SetAssetPath(const FString& InAssetPath)
{
	AssetPath = FPaths::Normalize(InAssetPath);
}

bool UParticleSystem::Validate(TArray<FString>* OutErrors) const
{
    bool bIsValid = true;
    if (Emitters.empty())
    {
        bIsValid = false;
        if (OutErrors)
        {
            OutErrors->push_back("Particle system must have at least one emitter.");
        }
    }

    for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
    {
        const UParticleEmitter* Emitter = Emitters[EmitterIndex];
        if (!Emitter)
        {
            bIsValid = false;
            if (OutErrors)
            {
                OutErrors->push_back("Particle system has a null emitter.");
            }
            continue;
        }
        if (!Emitter->Validate(OutErrors))
        {
            bIsValid = false;
        }
        if (Emitter->GetLODLevels().empty())
        {
            bIsValid = false;
            if (OutErrors)
            {
                OutErrors->push_back("Particle emitter has no LOD levels.");
            }
        }
    }

    return bIsValid;
}
UParticleSystem* UParticleSystem::CreateDefaultSpriteSystem()
{
    UParticleSystem* System = UObjectManager::Get().CreateObject<UParticleSystem>();
    if (!System)
    {
        return nullptr;
    }

    UParticleEmitter* Emitter = System->AddEmitter();
    if (!Emitter)
    {
        UObjectManager::Get().DestroyObject(System);
        return nullptr;
    }

    UParticleLODLevel* LODLevel = Emitter->AddLODLevel(0, 100000.0f);
    if (!LODLevel)
    {
        UObjectManager::Get().DestroyObject(System);
        return nullptr;
    }

	LODLevel->EnsureRequiredModule();
    LODLevel->AddModule<USpriteTypeData>();
    LODLevel->EnsureSpawnModule();
    LODLevel->AddModule<UParticleModuleLifetime>();
    LODLevel->AddModule<UParticleModuleLocation>();
    LODLevel->AddModule<UParticleModuleVelocity>();
    LODLevel->AddModule<UParticleModuleColor>();
    LODLevel->AddModule<UParticleModuleSize>();

    Emitter->CacheEmitterModuleInfo();

    return System;
}

// Function : Create default Mesh emitter particle system for detail-panel verification
// input : None
// output : New UParticleSystem with single emitter + UMeshTypeData using Dice mesh asset
//
// Cycle 11: CreateDefaultSpriteSystem과 동일 구조 + USpriteTypeData → UMeshTypeData 교체.
// Mesh asset은 기존 StaticMeshComponent 디폴트와 동일 (Asset/Mesh/Dice/Dice.obj) — 코드베이스에 존재 보장.
UParticleSystem* UParticleSystem::CreateDefaultMeshSystem()
{
    UParticleSystem* System = UObjectManager::Get().CreateObject<UParticleSystem>();
    if (!System)
    {
        return nullptr;
    }

    UParticleEmitter* Emitter = System->AddEmitter();
    if (!Emitter)
    {
        UObjectManager::Get().DestroyObject(System);
        return nullptr;
    }

    UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
    LODLevel->Level = 0;
    LODLevel->bEnabled = true;
    LODLevel->DistanceThreshold = 100000.0f;

    LODLevel->RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();

    // UMeshTypeData에 디폴트 mesh + override material.
    // apple_mid.obj 사용 이유:
    //   1. Dice.obj는 vt 4개 (cube corner)만 보유 — 6면 모두 동일 UV (0~1) → 텍스처 전체가 각 면에 반복 표시,
    //      UV mapping이 작동하는지 시각 검증 불가능.
    //   2. apple_mid.obj has expanded UVs; the imported material carries the BaseColor texture.
    // GetOrCreateMaterial은 빈 material 생성만 함 — DiffuseMap 등 params 채우려면 DeserializeMaterial 필수.
    UMeshTypeData* MeshTypeData = UObjectManager::Get().CreateObject<UMeshTypeData>();
    MeshTypeData->SetMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/apple_mid/apple_mid.obj"));
    const FString DemoMatPath = "Asset/Material/Auto/apple_mid_Mat_0.uasset";
    FResourceManager::Get().DeserializeMaterial(DemoMatPath);
    UMaterial* DemoMaterial = FResourceManager::Get().GetMaterial(DemoMatPath);
    if (!DemoMaterial)
    {
        DemoMaterial = FResourceManager::Get().GetMaterial("apple_mid_Mat_0");
    }
    if (DemoMaterial)
    {
        MeshTypeData->SetOverrideMaterial(true, DemoMaterial);
    }
    LODLevel->Modules.push_back(MeshTypeData);

    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleLocation>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleVelocity>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleColor>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSize>());

    Emitter->LODLevels.push_back(LODLevel);
    System->CacheEmitterModuleInfo();

    return System;
}

// Function : Create default Ribbon emitter particle system for detail-panel verification
// input : None
// output : New UParticleSystem with single emitter + URibbonTypeData (MaxTrailCount=1)
//
// Cycle 12: CreateDefaultMeshSystem과 동일 구조 + UMeshTypeData → URibbonTypeData.
// MaxTrailCount=1 + MaxParticleInTrail=64 (TypeData 기본값). Material 은 nullptr 시작 — 사용자가
// emitter detail panel 의 picker 로 선택. RenderRibbonEmitter 가 Material/Texture nullptr 시 default white SRV fallback.
UParticleSystem* UParticleSystem::CreateDefaultRibbonSystem()
{
    UParticleSystem* System = UObjectManager::Get().CreateObject<UParticleSystem>();
    if (!System)
    {
        return nullptr;
    }

    UParticleEmitter* Emitter = System->AddEmitter();
    if (!Emitter)
    {
        UObjectManager::Get().DestroyObject(System);
        return nullptr;
    }

    UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
    LODLevel->Level = 0;
    LODLevel->bEnabled = true;
    LODLevel->DistanceThreshold = 100000.0f;

    LODLevel->RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();

    // URibbonTypeData — TypeData 기본값 그대로 (MaxTrailCount=1, MaxParticleInTrail=64).
    // Material 은 사용자가 detail panel 에서 선택 — 본 시점은 nullptr.
    URibbonTypeData* RibbonTypeData = UObjectManager::Get().CreateObject<URibbonTypeData>();
    LODLevel->Modules.push_back(RibbonTypeData);

    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleLocation>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleVelocity>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleColor>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSize>());

    Emitter->LODLevels.push_back(LODLevel);
    System->CacheEmitterModuleInfo();

    return System;
}

// Function : Create default Beam emitter particle system for detail-panel verification
// input : None
// output : New UParticleSystem with single emitter + UBeamTypeData + Source/Target/Noise modules
//
// Cycle 13a/13b: CreateDefaultRibbonSystem 과 동일 구조 + URibbonTypeData → UBeamTypeData.
// Source/Target Component 은 nullptr 시작 (detail panel 의 picker 로 사용자가 선택). nullptr 시 fallback:
//   - SourceComponent nullptr → emitter 위치 (GetComponentWorldLocation)
//   - TargetComponent nullptr → Source + EmitterForward * FallbackDistance (UBeamTypeData::FallbackDistance)
// Noise 모듈은 기본값 (Frequency=4, NoiseRange=(0,30,30)) 으로 추가 — 즉시 lightning bolt 효과 가시화.
UParticleSystem* UParticleSystem::CreateDefaultBeamSystem()
{
    UParticleSystem* System = UObjectManager::Get().CreateObject<UParticleSystem>();
    if (!System)
    {
        return nullptr;
    }

    UParticleEmitter* Emitter = System->AddEmitter();
    if (!Emitter)
    {
        UObjectManager::Get().DestroyObject(System);
        return nullptr;
    }

    UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
    LODLevel->Level = 0;
    LODLevel->bEnabled = true;
    LODLevel->DistanceThreshold = 100000.0f;

    LODLevel->RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();

    // UBeamTypeData — 기본값 그대로 (MaxBeamCount=1, InterpolationPoints=0, FallbackDistance=100).
    // Material 은 사용자가 detail panel 에서 picker 로 선택 — 본 시점은 nullptr (RenderBeamEmitter 가 default white SRV fallback).
    UBeamTypeData* BeamTypeData = UObjectManager::Get().CreateObject<UBeamTypeData>();
    LODLevel->Modules.push_back(BeamTypeData);

    // Beam 의 Source / Target Component picker — detail panel 에서 사용자가 선택.
    // nullptr 인 동안은 fallback 으로 emitter 위치 + forward 방향 100 단위 strip 표시.
    UParticleModuleBeamSource* BeamSource = UObjectManager::Get().CreateObject<UParticleModuleBeamSource>();
    LODLevel->Modules.push_back(BeamSource);

    UParticleModuleBeamTarget* BeamTarget = UObjectManager::Get().CreateObject<UParticleModuleBeamTarget>();
    LODLevel->Modules.push_back(BeamTarget);

    // Cycle 13b: Noise 모듈 — 즉시 lightning bolt 가시화 (Frequency=4 default, NoiseRange=(0,30,30) default).
    UParticleModuleBeamNoise* BeamNoise = UObjectManager::Get().CreateObject<UParticleModuleBeamNoise>();
    LODLevel->Modules.push_back(BeamNoise);

    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleLocation>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleColor>());
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSize>());

    Emitter->LODLevels.push_back(LODLevel);
    System->CacheEmitterModuleInfo();

    return System;
}
