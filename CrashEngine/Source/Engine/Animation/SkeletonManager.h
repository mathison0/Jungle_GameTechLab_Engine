#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Resource/IAssetLoader.h"
#include "Animation/Skeleton.h"
#include <map>
#include <string>

// USkeleton 관련 객체의 생성, 조회, 수명 관리를 담당
class FSkeletonManager : public TSingleton<FSkeletonManager>, public IAssetLoader<USkeleton>
{
    friend class TSingleton<FSkeletonManager>;

    // path -> USkeleton* 캐시
    TMap<FString, USkeleton*> SkeletonCache;

public:
    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* /*InDevice*/) override {}
    virtual USkeleton* Load(const FString& Path) override { return LoadSkeleton(Path); }
    virtual USkeleton* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override { SkeletonCache.clear(); }

    USkeleton* LoadSkeleton(const FString& PathFileName);

private:
    FSkeletonManager() = default;
};