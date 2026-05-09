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

    bool IsInsideCacheFolder(const std::filesystem::path& Path)
    {
        for (const auto& Part : Path)
        {
            std::wstring Name = Part.wstring();
            std::transform(Name.begin(), Name.end(), Name.begin(), ::towlower);
            if (Name == L"cache") return true;
        }
        return false;
    }
}

FString FSkeletalMeshManager::GetBinaryFilePath(const FString& OriginalPath)
{
    const std::filesystem::path SrcPath = NormalizePathForCompare(OriginalPath);
    if (IsBinExtension(SrcPath)) return FPaths::FromPath(SrcPath);

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(OriginalPath, SourcePath, SubResource);
    
    if (!SubResource.empty())
    {
        // Skeletal Mesh 컨테이너 요청인 경우
        if (SubResource.find("Skel_") == 0 || SubResource.find("Mesh_") == 0)
        {
             return FPaths::BuildSubResourceCachePath(SourcePath, SubResource);
        }
    }
    return "";
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
        if (Path.filename().wstring().find(L".fbx_Mesh_") == std::wstring::npos) continue;

        FSkeletalMeshAssetListItem Item;
        FString Stem = FPaths::ToUtf8(Path.stem().wstring());
        Item.DisplayName = Stem;
        std::filesystem::path LogicalPath = Path.parent_path().parent_path() / FPaths::ToPath(Stem);
        Item.FullPath = FPaths::ToUtf8(LogicalPath.lexically_relative(ProjectRoot).generic_wstring());
        AvailableMeshFiles.push_back(std::move(Item));
    }
}

void FSkeletalMeshManager::ScanSkeletonCacheFiles()
{
    AvailableSkeletonFiles.clear();
    const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
    if (!std::filesystem::exists(AssetRoot)) return;
    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;
        const std::filesystem::path& Path = Entry.path();
        if (!IsBinExtension(Path) || !IsInsideCacheFolder(Path)) continue;
        if (Path.filename().wstring().find(L".fbx_Skel_") == std::wstring::npos) continue;

        FSkeletalMeshAssetListItem Item;
        FString Stem = FPaths::ToUtf8(Path.stem().wstring());
        Item.DisplayName = Stem;
        std::filesystem::path LogicalPath = Path.parent_path().parent_path() / FPaths::ToPath(Stem);
        Item.FullPath = FPaths::ToUtf8(LogicalPath.lexically_relative(ProjectRoot).generic_wstring());
        AvailableSkeletonFiles.push_back(std::move(Item));
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

bool FSkeletalMeshManager::TryImportSkeletalSubMesh(const FString& FBXSourcePath, const FString& SubResourceName, const FImportOptions* Options, USkeletalSubMesh* SubMesh, const FString& BinPath)
{
    if (!SubMesh) return false;
    bool bSuccess = Options ? FFBXImporter::ImportAndCacheAll(FBXSourcePath, *Options) : FFBXImporter::ImportAndCacheAll(FBXSourcePath, FImportOptions::Default());
    if (!bSuccess) return false;

    FWindowsBinReader Reader(BinPath);
    if (Reader.IsValid())
    {
        SubMesh->Serialize(Reader);
        return true;
    }
    return false;
}

USkeletalSubMesh* FSkeletalMeshManager::LoadSkeletalSubMesh(const FString& PathFileName)
{
    FString CacheKey = GetBinaryFilePath(PathFileName);
    if (CacheKey.empty()) return nullptr;

    auto It = SubMeshCache.find(CacheKey);
    if (It != SubMeshCache.end()) return It->second;

    USkeletalSubMesh* SubMesh = UObjectManager::Get().CreateObject<USkeletalSubMesh>();
    bool bNeedRebuild = true;

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);
    const std::filesystem::path BinPathW = FPaths::ToPath(CacheKey);
    const std::filesystem::path SourcePathW = FPaths::ToPath(SourcePath);

    if (std::filesystem::exists(BinPathW))
    {
        if (!std::filesystem::exists(SourcePathW) || std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(SourcePathW))
            bNeedRebuild = false;
    }

    if (!bNeedRebuild)
    {
        FWindowsBinReader Reader(CacheKey);
        if (Reader.IsValid()) SubMesh->Serialize(Reader);
        else bNeedRebuild = true;
    }

    if (bNeedRebuild)
    {
        if (!TryImportSkeletalSubMesh(SourcePath, SubResource, nullptr, SubMesh, CacheKey)) return nullptr;
    }

    SubMesh->InitResources(Device);
    SubMeshCache[CacheKey] = SubMesh;
    return SubMesh;
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName, bool bRefreshAssetLists)
{
    if (bRefreshAssetLists)
    {
        ScanMeshCacheFiles();
        ScanSkeletonCacheFiles();
        ScanFBXSourceFiles();
    }

    auto It = SkeletalMeshCache.find(PathFileName);
    if (It != SkeletalMeshCache.end()) return It->second;

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);

    // --- 추가: 캐시가 없는 경우 자동 임포트 시도 ---
    bool bHasCacheForThisSource = false;
    const std::filesystem::path RequestedSrcPath = NormalizePathForCompare(SourcePath);
    for (const auto& Item : AvailableMeshFiles)
    {
        FString ItemSource, ItemSub;
        FPaths::ParseSubResourcePath(Item.FullPath, ItemSource, ItemSub);
        if (NormalizePathForCompare(ItemSource) == RequestedSrcPath)
        {
            bHasCacheForThisSource = true;
            break;
        }
    }

    if (!bHasCacheForThisSource && std::filesystem::exists(FPaths::ToPath(SourcePath)))
    {
        if (FFBXImporter::ImportAndCacheAll(SourcePath, FImportOptions::Default()))
        {
            ScanMeshCacheFiles();
            ScanSkeletonCacheFiles();
        }
    }
    // ------------------------------------------

    USkeletalMesh* Container = UObjectManager::Get().CreateObject<USkeletalMesh>();
    TArray<USkeletalSubMesh*> LoadedSubMeshes;

    if (SubResource.find("Mesh_") == 0)
    {
        // 1. 단일 서브메시 요청
        USkeletalSubMesh* Sub = LoadSkeletalSubMesh(PathFileName);
        if (Sub)
        {
            Container->AddSubMesh(Sub);
            Container->SetSkeleton(Sub->GetSkeleton());
        }
    }
    else
    {
        // 2. FBX 전체 혹은 특정 스켈레톤 요청
        FString TargetSkeletonName = (SubResource.find("Skel_") == 0) ? SubResource.substr(5) : "";
        const std::filesystem::path SrcPath = NormalizePathForCompare(SourcePath);

        for (const auto& Item : AvailableMeshFiles)
        {
            FString ItemSource, ItemSub;
            FPaths::ParseSubResourcePath(Item.FullPath, ItemSource, ItemSub);
            if (NormalizePathForCompare(ItemSource) == SrcPath)
            {
                USkeletalSubMesh* Sub = LoadSkeletalSubMesh(Item.FullPath);
                if (Sub && Sub->GetSkeleton())
                {
                    if (TargetSkeletonName.empty())
                    {
                        // FBX 첫 번째 스켈레톤을 기준으로 잡음
                        TargetSkeletonName = Sub->GetSkeleton()->GetFName().ToString();
                    }

                    if (Sub->GetSkeleton()->GetFName().ToString() == TargetSkeletonName)
                    {
                        Container->AddSubMesh(Sub);
                        if (!Container->GetSkeleton()) Container->SetSkeleton(Sub->GetSkeleton());
                    }
                }
            }
        }
    }

    if (Container->GetSubMeshes().empty()) return nullptr;

    Container->SetAssetPathFileName(PathFileName);
    SkeletalMeshCache[PathFileName] = Container;
    return Container;
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName, const FImportOptions& Options, bool bRefreshAssetLists)
{
    // Options 지원은 추후 필요시 보강
    return LoadSkeletalMesh(PathFileName, bRefreshAssetLists);
}

USkeletalMesh* FSkeletalMeshManager::Find(const FString& Key)
{
    auto It = SkeletalMeshCache.find(Key);
    return (It != SkeletalMeshCache.end()) ? It->second : nullptr;
}

void FSkeletalMeshManager::Unload(const FString& Key)
{
    SkeletalMeshCache.erase(Key);
}

void FSkeletalMeshManager::ReleaseAllGPU()
{
    for (auto& [Key, Mesh] : SubMeshCache)
    {
        if (Mesh && Mesh->GetSkeletalSubMeshAsset() && Mesh->GetSkeletalSubMeshAsset()->RenderBuffer)
            Mesh->GetSkeletalSubMeshAsset()->RenderBuffer->Release();
    }
    SubMeshCache.clear();
    SkeletalMeshCache.clear();
}
