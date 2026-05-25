#pragma once

#include "Object/Object.h"
#include "Particle/ParticleModules.h"
#include "Particle/ParticleModuleTypeData.h"

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY(UParticleLODLevel, UObject)
    ~UParticleLODLevel() override;
    void PostDuplicate(UObject* Original) override;
	UParticleModuleRequired* EnsureRequiredModule();
    UParticleModuleSpawn* EnsureSpawnModule();

	template <typename T>
    T* AddModule()
    {
        static_assert(std::is_base_of_v<UParticleModule, T>, "T must derive from UParticleModule");

        T* NewModule = UObjectManager::Get().CreateObject<T>();
        if (!NewModule)
            return nullptr;

        Modules.push_back(NewModule);
        CacheModuleLists();
        return NewModule;
    }

    void RemoveModule(UParticleModule* Module);
    void ClearModules();
    bool Validate(TArray<FString>* OutErrors = nullptr) const;
	void CacheModuleLists();

	int32 GetLevel() const { return Level; }
	bool IsEnabled() const { return bEnabled; }
	float GetDistanceThreshold() const { return DistanceThreshold; }
	UParticleModuleRequired* GetRequiredModule() const { return RequiredModule; }
	UParticleModuleSpawn* GetSpawnModule() const { return SpawnModule; }
	const TArray<UParticleModule*>& GetModules() const { return Modules; }
	const TArray<UParticleModule*>& GetSpawnModules() const { return SpawnModules; }
	const TArray<UParticleModule*>& GetUpdateModules() const { return UpdateModules; }
	UParticleModuleTypeDataBase* GetTypeDataModule() const { return TypeDataModule; }

	// Resolve effective render mode: TypeDataModule is single source of truth.
	// Falls back to RequiredModule.RenderMode when TypeData is absent, then Sprite.
	EParticleEmitterRenderMode GetEffectiveRenderMode() const;

	UPROPERTY(DisplayName = "Level")
	int32 Level = 0;

	UPROPERTY(DisplayName = "Enabled")
	bool bEnabled = true;

	UPROPERTY(DisplayName = "Distance Threshold", Min = 0.0f)
	float DistanceThreshold = 100000.0f;

	UPROPERTY(DisplayName = "Required Module")
	UParticleModuleRequired* RequiredModule = nullptr;

	UPROPERTY(DisplayName = "Modules")
	TArray<UParticleModule*> Modules;

	// silent bug ι 회피: UPROPERTY로 마크하지 않으면 .particlesystem 저장-로드 후 nullptr이 되어
	// 모든 emitter가 Sprite로 fallback되는 silent regression이 발생한다.
	UPROPERTY(DisplayName = "TypeData Module")
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

private:
	UParticleModuleSpawn* SpawnModule = nullptr;
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
};

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY(UParticleEmitter, UObject)

	~UParticleEmitter() override;
    void PostDuplicate(UObject* Original) override;
	UParticleLODLevel* AddLODLevel(int32 Level, float DistanceThreshold);
    void RemoveLODLevel(int32 Index);
    void ClearLODLevels();
    void SortLODLevelsByDistance();
    bool Validate(TArray<FString>* OutErrors = nullptr) const;

	void CacheEmitterModuleInfo();
	UParticleLODLevel* GetLODLevel(int32 Index) const;
	int32 SelectLODLevel(float Distance) const;

	const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }
	int32 GetParticleSize() const { return ParticleSize; }
	int32 GetMaxActiveParticleCount() const { return MaxActiveParticles; }

	UPROPERTY(DisplayName = "LOD Levels")
	TArray<UParticleLODLevel*> LODLevels;


private:
	int32 ParticleSize = sizeof(FBaseParticle);
	int32 MaxActiveParticles = 128;
};

UCLASS()
class UParticleSystem : public UObject
{
public:
	GENERATED_BODY(UParticleSystem, UObject)
    ~UParticleSystem() override;
    void PostDuplicate(UObject* Original) override;

	const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }
    UParticleEmitter* AddEmitter();
    void RemoveEmitter(int32 Index);
    void ClearEmitters();
	void CacheEmitterModuleInfo();
    bool Validate(TArray<FString>* OutErrors = nullptr) const;
    static UParticleSystem* CreateDefaultSpriteSystem();

	UPROPERTY(DisplayName = "Update Time FPS", Min = 0.0f)
	float UpdateTimeFPS = 60.0f;

	UPROPERTY(DisplayName = "Warmup Time - beware hitches!", Min = 0.0f)
	float WarmupTime = 0.0f;

	UPROPERTY(DisplayName = "Warmup Tick Rate", Min = 0.0f)
	float WarmupTickRate = 0.0f;

	UPROPERTY(DisplayName = "Seconds Before Inactive", Min = 0.0f)
	float SecondsBeforeInactive = 0.0f;

	UPROPERTY(DisplayName = "Orient ZAxis Toward Camera")
	bool bOrientZAxisTowardCamera = false;

	UPROPERTY(DisplayName = "System Update Mode")
	int32 SystemUpdateMode = 0;

	UPROPERTY(DisplayName = "Thumbnail Warmup", Min = 0.0f)
	float ThumbnailWarmup = 1.0f;

	UPROPERTY(DisplayName = "Use Realtime Thumbnail")
	bool bUseRealtimeThumbnail = false;

	UPROPERTY(DisplayName = "LODDistance Check Time", Min = 0.0f)
	float LODDistanceCheckTime = 0.25f;

	UPROPERTY(DisplayName = "LODDistances")
	TArray<float> LODDistances = { 0.0f, 1000.0f };

	UPROPERTY(DisplayName = "LODSettings")
	TArray<int32> LODSettings = { 0, 1 };

	UPROPERTY(DisplayName = "LODMethod")
	int32 LODMethod = 0;

	UPROPERTY(DisplayName = "Emitters")
    TArray<UParticleEmitter*> Emitters;
};
