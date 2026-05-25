#include "Particle/ParticleSystem.h"

#include <algorithm>

#include "Particle/ParticleModuleTypeData.h"

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

		// TypeData는 별도 슬롯에 캐싱하고 SpawnModules/UpdateModules에는 넣지 않음 (UE Cascade 패턴).
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

    UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
    LODLevel->Level = 0;
    LODLevel->bEnabled = true;
    LODLevel->DistanceThreshold = 100000.0f;

    LODLevel->RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();
    // USpriteTypeData를 명시 등록하여 기본 Sprite asset도 TypeData 시스템 안으로 편입.
    // CacheModuleLists()가 Cast<UParticleModuleTypeDataBase>로 자동 캐싱한다.
    LODLevel->Modules.push_back(UObjectManager::Get().CreateObject<USpriteTypeData>());
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
