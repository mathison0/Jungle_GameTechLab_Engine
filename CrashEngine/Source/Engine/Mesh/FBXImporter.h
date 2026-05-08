#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/MeshImporterCommon.h"
#include <memory>

struct FSkeletalMesh;
class USkeleton;
struct FStaticMaterial;

struct FFBXImporter
{
    struct FImportedSkeletalMesh
    {
        FString Name;
        std::unique_ptr<FSkeletalMesh> MeshData;
        FString SkeletonName;
    };

    struct FImportedAssets
    {
        TArray<FImportedSkeletalMesh> SkeletalMeshes;
        TArray<USkeleton*> Skeletons;
        TArray<FStaticMaterial> Materials;
    };

    // FBX 파일로부터 모든 메시와 스켈레톤 데이터를 추출합니다.
    static bool ImportAll(const FString& FBXFilePath, const FImportOptions& Options, FImportedAssets& OutAssets);

    // FBX 파일을 분석하여 발견된 모든 리소스를 각각의 .bin 파일로 캐시 폴더에 저장합니다.
    static bool ImportAndCacheAll(const FString& FBXFilePath, const FImportOptions& Options);
};
