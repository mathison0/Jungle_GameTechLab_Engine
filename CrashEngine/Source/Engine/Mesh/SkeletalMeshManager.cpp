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
    void ParseFBXPath(const FString& InPath, FString& OutSourcePath, FString& OutSubResource)
    {
        size_t Pos = InPath.find_last_of(':');
        if (Pos != FString::npos)
        {
            OutSourcePath = InPath.substr(0, Pos);
            OutSubResource = InPath.substr(Pos + 1);
        }
        else
        {
            OutSourcePath = InPath;
            OutSubResource = "";
        }
    }

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

    FString BuildSubResourceCachePath(const FString& SourceFBX, const FString& SubResource)
    {
        const std::filesystem::path SrcPath = NormalizePathForCompare(SourceFBX);
        const std::filesystem::path CacheDir = SrcPath.parent_path() / L"Cache";
        FPaths::CreateDir(CacheDir.wstring());

        FString FileName = FPaths::ToUtf8(SrcPath.filename().wstring());
        FString CacheFileName = FileName + ":" + SubResource + ".bin";
        
        std::filesystem::path CacheFile = CacheDir / FPaths::ToPath(CacheFileName);
        return FPaths::FromPath(CacheFile.lexically_normal());
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
    ParseFBXPath(OriginalPath, SourcePath, SubResource);
    if (SubResource.empty()) return "";

    return BuildSubResourceCachePath(SourcePath, SubResource);
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

        // .bin 파일 중 SkeletalMesh 용인지 확인하는 로직이 필요할 수 있으나,
        // 현재는 Cache 폴더 내 모든 .bin을 후보로 등록 (파일명에 :가 포함됨)
        if (Path.filename().wstring().find(L':') == std::wstring::npos) continue;

        FSkeletalMeshAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
        Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
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
        // 테스트로 무조건 Armature만 로드하게 설정
        Item.FullPath = Item.FullPath + ":Armature";
        // 테스트로 무조건 Armature만 로드하게 설정
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
    const FString CacheKey = GetBinaryFilePath(PathFileName);
    if (CacheKey.empty()) return nullptr;

    SkeletalMeshCache.erase(CacheKey);

    USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    
    FString SourcePath, SubResource;
    ParseFBXPath(PathFileName, SourcePath, SubResource);

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
    const FString CacheKey = GetBinaryFilePath(PathFileName);
    if (CacheKey.empty()) return nullptr;

    auto It = SkeletalMeshCache.find(CacheKey);
    if (It != SkeletalMeshCache.end()) return It->second;

    USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    bool bNeedRebuild = true;

    FString SourcePath, SubResource;
    ParseFBXPath(PathFileName, SourcePath, SubResource);

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
