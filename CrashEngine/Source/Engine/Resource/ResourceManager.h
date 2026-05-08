// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include <memory>

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include "Resource/IAssetLoader.h"

struct ID3D11Device;

// FResourceManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FResourceManager : public TSingleton<FResourceManager>
{
    friend class TSingleton<FResourceManager>;

    // Nested Loaders
    template <typename T>
    class TGenericResourceLoader : public IAssetLoader<T>
    {
        TMap<FString, T>& Resources;
        ID3D11Device* Device = nullptr;

    public:
        TGenericResourceLoader(TMap<FString, T>& InResources) : Resources(InResources) {}

        virtual void SetDevice(ID3D11Device* InDevice) override { Device = InDevice; }
        virtual T* Load(const FString& Path) override { return nullptr; } // Specific logic in subclasses or FResourceManager
        virtual T* Find(const FString& Key) override
        {
            auto It = Resources.find(Key);
            return (It != Resources.end()) ? &It->second : nullptr;
        }
        virtual void Unload(const FString& Key) override { Resources.erase(Key); }
        virtual void UnloadAll() override { Resources.clear(); }
    };

    class FFontLoader : public TGenericResourceLoader<FFontResource>
    {
    public:
        using TGenericResourceLoader<FFontResource>::TGenericResourceLoader;
        virtual FFontResource* Load(const FString& Path) override;
    };

    class FTextureLoader : public TGenericResourceLoader<FTextureResource>
    {
    public:
        using TGenericResourceLoader<FTextureResource>::TGenericResourceLoader;
        virtual FTextureResource* Load(const FString& Path) override;
    };

    class FParticleLoader : public TGenericResourceLoader<FParticleResource>
    {
    public:
        using TGenericResourceLoader<FParticleResource>::TGenericResourceLoader;
        virtual FParticleResource* Load(const FString& Path) override;
    };

public:
    // Resource.ini에서 경로/그리드 정보 로드 후 GPU 리소스 생성
    void LoadFromFile(const FString& Path, ID3D11Device* InDevice);

    // GPU 리소스 로드 (Device 필요)
    bool LoadGPUResources(ID3D11Device* Device);

    // 모든 GPU 리소스 해제
    void ReleaseGPUResources();

    void SetDevice(ID3D11Device* Device);

    // --- Font ---
    FFontResource* FindFont(const FName& FontName);
    const FFontResource* FindFont(const FName& FontName) const;
    void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns = 16, uint32 Rows = 16);
    IAssetLoader<FFontResource>& GetFontLoader() { return *FontLoader; }

    // --- Font names ---
    TArray<FString> GetFontNames() const;

    // --- Particle ---
    FParticleResource* FindParticle(const FName& ParticleName);
    const FParticleResource* FindParticle(const FName& ParticleName) const;
    void RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);
    IAssetLoader<FParticleResource>& GetParticleLoader() { return *ParticleLoader; }

    // --- Particle names ---
    TArray<FString> GetParticleNames() const;

    // --- Texture (단일 정적 이미지, 1x1 atlas) ---
    FTextureResource* FindTexture(const FName& TextureName);
    const FTextureResource* FindTexture(const FName& TextureName) const;
    void RegisterTexture(const FName& TextureName, const FString& InPath);
    IAssetLoader<FTextureResource>& GetTextureLoader() { return *TextureLoader; }

    // --- Texture names ---
    TArray<FString> GetTextureNames() const;
    TArray<FString> GetEditorTextureNames() const;

private:
    FResourceManager();
    ~FResourceManager() { ReleaseGPUResources(); }

    TMap<FString, FFontResource> FontResources;
    TMap<FString, FParticleResource> ParticleResources;
    TMap<FString, FTextureResource> TextureResources;

    std::unique_ptr<FFontLoader> FontLoader;
    std::unique_ptr<FParticleLoader> ParticleLoader;
    std::unique_ptr<FTextureLoader> TextureLoader;
};

