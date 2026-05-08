#include "Mesh/FBXImporter.h"
#include "Mesh/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Core/Logging/LogMacros.h"
#include "Object/Object.h"
#include <filesystem>

namespace
{
    FString BuildSubResourceCachePath(const FString& FBXFilePath, const FString& SubResourceName)
    {
        const std::filesystem::path SourcePath = std::filesystem::path(FPaths::ToPath(FBXFilePath)).lexically_normal();
        const std::filesystem::path CacheDir = SourcePath.parent_path() / L"Cache";
        FPaths::CreateDir(CacheDir.wstring());

        FString FileName = FPaths::ToUtf8(SourcePath.filename().wstring());
        FString CacheFileName = FileName + ":" + SubResourceName + ".bin";
        
        std::filesystem::path CacheFile = CacheDir / FPaths::ToPath(CacheFileName);
        return FPaths::FromPath(CacheFile.lexically_normal());
    }
}

bool FFBXImporter::ImportAll(const FString& FBXFilePath, const FImportOptions& Options, FImportedAssets& OutAssets)
{
    // TODO: FBX SDK를 이용한 실제 파싱 로직 구현 (메시 노드 순회, 스켈레톤 추출 등)
    // 현재는 스텁 상태로 빈 데이터를 반환하거나 실패를 알립니다.
    UE_LOG(LogTemp, Warning, "FFBXImporter::ImportAll: FBX SDK integration is not yet implemented.");
    return false; 
}

bool FFBXImporter::ImportAndCacheAll(const FString& FBXFilePath, const FImportOptions& Options)
{
    FImportedAssets Assets;
    if (!ImportAll(FBXFilePath, Options, Assets))
    {
        return false;
    }

    // 1. Skeletal Meshes 캐싱
    for (auto& ImportedMesh : Assets.SkeletalMeshes)
    {
        FString BinPath = BuildSubResourceCachePath(FBXFilePath, ImportedMesh.Name);
        
        // USkeletalMesh 임시 객체를 생성하여 직렬화 수행 (파일 저장용)
        // 실제 엔진 로드는 FSkeletalMeshManager가 담당
        USkeletalMesh* TempMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
        TempMesh->SetSkeletalMeshAsset(ImportedMesh.MeshData.release());
        
        // TODO: 스켈레톤 이름과 매핑하는 로직 추가 필요
        
        FWindowsBinWriter Writer(BinPath);
        if (Writer.IsValid())
        {
            TempMesh->Serialize(Writer);
        }
        
        // TempMesh는 UObjectManager에 의해 관리되거나 수동으로 제거 (여기서는 임시)
        // UObjectManager::Get().DestroyObject(TempMesh); // Destroy 기능이 있다면 사용
    }

    // 2. Skeletons 캐싱
    for (auto* Skeleton : Assets.Skeletons)
    {
        FString SubName = Skeleton->GetFName().ToString();
        FString BinPath = BuildSubResourceCachePath(FBXFilePath, SubName);
        
        FWindowsBinWriter Writer(BinPath);
        if (Writer.IsValid())
        {
            Skeleton->Serialize(Writer);
        }
    }

    return true;
}
