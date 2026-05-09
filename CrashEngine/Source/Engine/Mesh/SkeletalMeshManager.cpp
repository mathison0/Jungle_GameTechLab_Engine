#include "Mesh/SkeletalMeshManager.h"
#include "Core/Logging/LogMacros.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/FBXImporter.h"
#include "Materials/Material.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include <filesystem>
#include <algorithm>
#include <cwctype>

namespace
{
    std::filesystem::path NormalizePathForCompare(const FString& InPath)
    {
        std::filesystem::path Path = FPaths::ToPath(InPath).lexically_normal();
        std::error_code Ec;
        std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
        return Ec ? Path : Canonical;
    }

    bool IsBinExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext == L".bin";
    }

    bool IsFBXExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext == L".fbx";
    }

    bool IsOBJExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext == L".obj";
    }

    bool IsInsideCacheFolder(const std::filesystem::path& Path)
    {
        for (const auto& Part : Path)
        {
            std::wstring Name = Part.wstring();
            std::transform(Name.begin(), Name.end(), Name.begin(), ::towlower);
            if (Name == L"cache")
            {
                return true;
            }
        }
        return false;
    }
}

FString FSkeletalMeshManager::GetBinaryFilePath(const FString& OriginalPath)
{
    const std::filesystem::path SrcPath = NormalizePathForCompare(OriginalPath);
    if (IsBinExtension(SrcPath))
    {
        return FPaths::FromPath(SrcPath);
    }

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(OriginalPath, SourcePath, SubResource);
    
    // 만약 서브리소스가 지정되었다면 Mesh_ 접두사가 붙어있는지 확인하고 붙여줍니다.
    if (!SubResource.empty())
    {
        if (SubResource.find("Mesh_") != 0)
        {
            SubResource = "Mesh_" + SubResource;
        }
    }
    else
    {
        // 서브리소스가 지정되지 않았다면 (예: "Hero.fbx")
        // 현재 스캔된 에셋 목록에서 해당 소스 파일을 사용하는 첫 번째 'Mesh_' 리소스를 찾습니다.
        for (const auto& Item : FSkeletalMeshManager::Get().AvailableMeshFiles)
        {
            FString ItemSource, ItemSub;
            FPaths::ParseSubResourcePath(Item.FullPath, ItemSource, ItemSub);
            if (NormalizePathForCompare(ItemSource) == SrcPath && ItemSub.find("Mesh_") == 0)
            {
                SourcePath = ItemSource;
                SubResource = ItemSub;
                break;
            }
        }
    }

    if (SubResource.empty())
    {
        return "";
    }

    return FPaths::BuildSubResourceCachePath(SourcePath, SubResource);
}

void FSkeletalMeshManager::ScanMeshCacheFiles()
{
    AvailableMeshFiles.clear();

    const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
    if (!std::filesystem::exists(AssetRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;

        const std::filesystem::path& Path = Entry.path();
        if (!IsBinExtension(Path) || !IsInsideCacheFolder(Path)) continue;

        // .bin 파일 중 SkeletalMesh 용인지 확인하는 로직
        // FBX 캐시 파일은 "파일명.fbx_Mesh_서브리소스.bin" 형식을 가집니다.
        if (Path.filename().wstring().find(L".fbx_Mesh_") == std::wstring::npos &&
            Path.filename().wstring().find(L".obj_Mesh_") == std::wstring::npos) continue;

        FSkeletalMeshAssetListItem Item;
        FString Stem = FPaths::ToUtf8(Path.stem().wstring());
        Item.DisplayName = Stem;
        
        // 물리적인 캐시 경로(Asset/.../Cache/abc.bin) 대신 
        // 논리적인 경로(Asset/.../abc.fbx_Mesh_Sub)를 FullPath로 저장합니다.
        std::filesystem::path LogicalPath = Path.parent_path().parent_path() / FPaths::ToPath(Stem);
        Item.FullPath = FPaths::ToUtf8(LogicalPath.lexically_relative(ProjectRoot).generic_wstring());
        
        AvailableMeshFiles.push_back(std::move(Item));
    }
}

void FSkeletalMeshManager::ScanFBXSourceFiles()
{
    AvailableFBXFiles.clear();

    const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
    if (!std::filesystem::exists(AssetRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;

        const std::filesystem::path& Path = Entry.path();
        if (!IsFBXExtension(Path) || IsInsideCacheFolder(Path)) continue;

        FSkeletalMeshAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
        Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
        AvailableFBXFiles.push_back(std::move(Item));
    }
}

bool FSkeletalMeshManager::TryImportSkeletalMesh(const FString& FBXSourcePath, const FString& SubResourceName, const FImportOptions* Options, USkeletalMesh* SkeletalMesh, const FString& BinPath)
{
    if (!SkeletalMesh) return false;

    // FBX 파일 전체를 분석하여 캐시 생성
    bool bSuccess = Options 
        ? FFBXImporter::ImportAndCacheAll(FBXSourcePath, *Options)
        : FFBXImporter::ImportAndCacheAll(FBXSourcePath, FImportOptions::Default());

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, "Failed to import and cache FBX assets from: %s", FBXSourcePath.c_str());
        return false;
    }

    // 캐시 생성 후 요청된 .bin 로드
    FWindowsBinReader Reader(BinPath);
    if (Reader.IsValid())
    {
        SkeletalMesh->Serialize(Reader);
        return true;
    }

    return false;
}

USkeletalMesh* FSkeletalMeshManager::Find(const FString& Key)
{
    const FString CacheKey = GetBinaryFilePath(Key);
    auto It = SkeletalMeshCache.find(CacheKey);
    return (It != SkeletalMeshCache.end()) ? It->second : nullptr;
}

void FSkeletalMeshManager::Unload(const FString& Key)
{
    const FString CacheKey = GetBinaryFilePath(Key);
    auto It = SkeletalMeshCache.find(CacheKey);
    if (It != SkeletalMeshCache.end())
    {
        USkeletalMesh* Mesh = It->second;
        if (Mesh && Mesh->GetSkeletalMeshAsset())
        {
            if (Mesh->GetSkeletalMeshAsset()->RenderBuffer)
            {
                Mesh->GetSkeletalMeshAsset()->RenderBuffer->Release();
            }
        }
        SkeletalMeshCache.erase(It);
    }
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName, const FImportOptions& Options, bool bRefreshAssetLists)
{
    FString CacheKey = GetBinaryFilePath(PathFileName);
    
    // 캐시가 없으면 원본 파일이 있는지 확인하고 임포트를 시도합니다.
    if (CacheKey.empty())
    {
        FString SourcePath, SubResource;
        FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);
        if (std::filesystem::exists(FPaths::ToPath(SourcePath)))
        {
            if (FFBXImporter::ImportAndCacheAll(SourcePath, Options))
            {
                ScanMeshCacheFiles();
                CacheKey = GetBinaryFilePath(PathFileName);
            }
        }
    }

    if (CacheKey.empty()) return nullptr;

    SkeletalMeshCache.erase(CacheKey);

    USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    
    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);

    if (!TryImportSkeletalMesh(SourcePath, SubResource, &Options, Mesh, CacheKey))
    {
        return nullptr;
    }

    Mesh->InitResources(Device);
    SkeletalMeshCache[CacheKey] = Mesh;

    if (bRefreshAssetLists) ScanMeshCacheFiles();
    return Mesh;
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName, bool bRefreshAssetLists)
{
    FString CacheKey = GetBinaryFilePath(PathFileName);

    // 캐시가 없으면 원본 파일이 있는지 확인하고 임포트를 시도합니다.
    if (CacheKey.empty())
    {
        FString SourcePath, SubResource;
        FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);
        if (std::filesystem::exists(FPaths::ToPath(SourcePath)))
        {
            if (FFBXImporter::ImportAndCacheAll(SourcePath, FImportOptions::Default()))
            {
                ScanMeshCacheFiles();
                CacheKey = GetBinaryFilePath(PathFileName);
            }
        }
    }

    if (CacheKey.empty()) return nullptr;

    auto It = SkeletalMeshCache.find(CacheKey);
    if (It != SkeletalMeshCache.end()) return It->second;

    USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    bool bNeedRebuild = true;

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);

    const std::filesystem::path BinPathW = FPaths::ToPath(CacheKey);
    const std::filesystem::path SourcePathW = FPaths::ToPath(SourcePath);

    if (std::filesystem::exists(BinPathW))
    {
        if (!std::filesystem::exists(SourcePathW) || 
            std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(SourcePathW))
        {
            bNeedRebuild = false;
        }
    }

    if (!bNeedRebuild)
    {
        FWindowsBinReader Reader(CacheKey);
        if (Reader.IsValid())
        {
            Mesh->Serialize(Reader);
        }
        else
        {
            bNeedRebuild = true;
        }
    }

    if (bNeedRebuild)
    {
        if (!TryImportSkeletalMesh(SourcePath, SubResource, nullptr, Mesh, CacheKey))
        {
            return nullptr;
        }
    }

    Mesh->InitResources(Device);
    SkeletalMeshCache[CacheKey] = Mesh;

    if (bRefreshAssetLists) ScanMeshCacheFiles();
    return Mesh;
}

void FSkeletalMeshManager::ReleaseAllGPU()
{
    for (auto& [Key, Mesh] : SkeletalMeshCache)
    {
        if (Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->RenderBuffer)
        {
            Mesh->GetSkeletalMeshAsset()->RenderBuffer->Release();
        }
    }
    SkeletalMeshCache.clear();
}
