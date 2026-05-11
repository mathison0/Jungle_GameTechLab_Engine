#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/MeshImporterCommon.h"
#include "Math/Transform.h"
#include "ThirdParty/FBX_SDK/include/fbxsdk.h"
#include "Object/FName.h"
#include "Mesh/StaticMeshAsset.h"
#include <memory>

struct FSkeletalSubMesh;
class USkeleton;
class UAnimationSequence;

struct FFBXImporter
{
    struct FImportedSkeletalMesh
    {
        FName Name;
        FSkeletalSubMesh* MeshData;
        FName SkeletonName;

        FImportedSkeletalMesh(FName InName, FSkeletalSubMesh* InMeshData, FName InSkeletonName);
        ~FImportedSkeletalMesh();

        FImportedSkeletalMesh(const FImportedSkeletalMesh&) = delete;
        FImportedSkeletalMesh& operator=(const FImportedSkeletalMesh&) = delete;
    };

    struct FImportedFBXAssets
    {
        TArray<FImportedSkeletalMesh*> SkeletalMeshes;
        TArray<USkeleton*> Skeletons;
        TArray<UAnimationSequence*> Animations;
        TArray<FStaticMaterial> Materials;

        FImportedFBXAssets();
        ~FImportedFBXAssets();

        FImportedFBXAssets(const FImportedFBXAssets&) = delete;
        FImportedFBXAssets& operator=(const FImportedFBXAssets&) = delete;

        FImportedFBXAssets(FImportedFBXAssets&& Other) noexcept;
        FImportedFBXAssets& operator=(FImportedFBXAssets&& Other) noexcept;
    };

    // FBX 파일을 분석하여 발견된 모든 리소스를 각각의 .bin 파일로 캐시 폴더에 저장합니다.
    static bool ImportAndCacheAll(const FString& FBXFilePath, const FImportOptions& Options);

    // FBX 파일로부터 모든 메시와 스켈레톤 데이터를 추출합니다.
    static bool ImportAll(const FString& FBXFilePath, const FImportOptions& Options, FImportedFBXAssets& OutAssets);

private:
    static void Initialize();
    static void ExtractBoneNodeRecursive(FbxNode* Node, int ParentIndex, USkeleton* OutSkeleton);
    static void ExtractMeshAndSkinning(FbxNode* Node, const FImportOptions& Options, FImportedFBXAssets& InAsset);
    static void ExtractAnimations(FbxScene* Scene, FImportedFBXAssets& OutAssets);  // PlaceHolder, 아직 미구현
    static std::unique_ptr<FSkeletalSubMesh> ParseGeometry(FbxMesh* InFbxMesh, const FImportOptions& Options);
    static FTransform GetTransformFromNode(FbxNode* Node);
    static FTransform GetTransformFromMatrix(const FMatrix& Matrix);
    static FMatrix ConvertFbxMatrix(const FbxMatrix& Matrix);
    static FMatrix ConvertFbxMatrix(const FbxAMatrix& Matrix);
    static void ApplyBindPoseToSkeleton(FbxMesh* InFbxMesh, USkeleton* InSkeleton);
    static void ExtractWeights(FbxCluster* InCluster, int InBoneIndex);
    static void ApplyWeightsToSkeleton(FSkeletalSubMesh* InMesh);
    
    static bool bInitialized;
    static FbxManager* SdkManager;
    
    // FBX 추출 과정에서 사용하는 임시 컨테이너들
    static TArray<TArray<int>> CtrlPointToVertexIndex;      // FBX의 mesh vertex가 FSkeletalMesh의 몇번 vertex들에 해당하는지 보관
    using FBoneWeighting = std::pair<uint16, float>;        // id, weight 순
    static TArray<TArray<FBoneWeighting>> BoneWeighting;    // index = FSkeletalMesh의 vertex index
};
