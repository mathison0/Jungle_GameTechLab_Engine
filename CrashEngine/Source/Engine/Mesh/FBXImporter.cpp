#include "Mesh/FBXImporter.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/Skeleton.h"
#include "Animation/AnimationSequence.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Core/Logging/LogMacros.h"
#include "Object/Object.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <ranges>
#include <fstream>
#include <unordered_map>

namespace
{
    FVector GetSafeInverseScale(const FVector& Scale)
    {
        return FVector(
            std::abs(Scale.X) > 1e-6f ? 1.0f / Scale.X : 1.0f,
            std::abs(Scale.Y) > 1e-6f ? 1.0f / Scale.Y : 1.0f,
            std::abs(Scale.Z) > 1e-6f ? 1.0f / Scale.Z : 1.0f);
    }

    // 테스트용 함수
    void PrintNode(FbxNode* pNode) {
        const char* nodeName = pNode->GetName();
        FbxDouble3 translation = pNode->LclTranslation.Get();
        FbxDouble3 rotation = pNode->LclRotation.Get();
        FbxDouble3 scaling = pNode->LclScaling.Get();
        
        FString attrStr; 
        auto attr = pNode->GetNodeAttribute();
        if (attr)
        {
            switch (attr->GetAttributeType())
            {
            case FbxNodeAttribute::eUnknown:
                attrStr = "eUnknown"; break;
            case FbxNodeAttribute::eNull:
                attrStr = "eNull"; break;
            case FbxNodeAttribute::eMarker:
                attrStr = "eMarker"; break;
            case FbxNodeAttribute::eSkeleton:
                attrStr = "eSkeleton"; break;
            case FbxNodeAttribute::eMesh:
                attrStr = "eMesh"; break;
            case FbxNodeAttribute::eNurbs:
                attrStr = "eNurbs"; break;
            case FbxNodeAttribute::ePatch:
                attrStr = "ePatch"; break;
            case FbxNodeAttribute::eCamera:
                attrStr = "eCamera"; break;
            case FbxNodeAttribute::eCameraStereo:
                attrStr = "eCameraStereo"; break;
            case FbxNodeAttribute::eCameraSwitcher:
                attrStr = "eCameraSwitcher"; break;
            case FbxNodeAttribute::eLight:
                attrStr = "eLight"; break;
            case FbxNodeAttribute::eOpticalReference:
                attrStr = "eOpticalReference"; break;
            case FbxNodeAttribute::eOpticalMarker:
                attrStr = "eOpticalMarker"; break;
            case FbxNodeAttribute::eNurbsCurve:
                attrStr = "eNurbsCurve"; break;
            case FbxNodeAttribute::eTrimNurbsSurface:
                attrStr = "eTrimNurbsSurface"; break;
            case FbxNodeAttribute::eBoundary:
                attrStr = "eBoundary"; break;
            case FbxNodeAttribute::eNurbsSurface:
                attrStr = "eNurbsSurface"; break;
            case FbxNodeAttribute::eShape:
                attrStr = "eShape"; break;
            case FbxNodeAttribute::eLODGroup:
                attrStr = "eLODGroup"; break;
            case FbxNodeAttribute::eSubDiv:
                attrStr = "eSubDiv"; break;
            case FbxNodeAttribute::eCachedEffect:
                attrStr = "eCachedEffect"; break;
            case FbxNodeAttribute::eLine:
                attrStr = "eLine"; break;
            default:
                attrStr = "ERROR";
            }
        }

        UE_LOG(FBXImporter, Warning, 
            "<node name='%s' attribute = '%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
            nodeName, attrStr.c_str(),
            translation[0], translation[1], translation[2],
            rotation[0], rotation[1], rotation[2],
            scaling[0], scaling[1], scaling[2]
            );

        for(int j = 0; j < pNode->GetChildCount(); j++)
            PrintNode(pNode->GetChild(j));
    }
} // namespace

bool FFBXImporter::bInitialized = false;
FbxManager* FFBXImporter::SdkManager = nullptr;
TArray<TArray<int>> FFBXImporter::CtrlPointToVertexIndex;
TArray<TArray<FFBXImporter::FBoneWeighting>> FFBXImporter::BoneWeighting;

FFBXImporter::FImportedSkeletalMesh::FImportedSkeletalMesh(FName InName, FSkeletalSubMesh* InMeshData, FName InSkeletonName)
    : Name(InName), MeshData(InMeshData), SkeletonName(InSkeletonName)
{
}

FFBXImporter::FImportedSkeletalMesh::~FImportedSkeletalMesh()
{
    delete MeshData;
}

FFBXImporter::FImportedFBXAssets::FImportedFBXAssets()
{
}

FFBXImporter::FImportedFBXAssets::~FImportedFBXAssets()
{
    for (auto* Mesh : SkeletalMeshes) delete Mesh;
    SkeletalMeshes.clear();
    // UObject들은 UObjectManager가 관리하므로 여기서 직접 해제하지 않음
}

FFBXImporter::FImportedFBXAssets::FImportedFBXAssets(FImportedFBXAssets&& Other) noexcept
    : SkeletalMeshes(std::move(Other.SkeletalMeshes)),
      Skeletons(std::move(Other.Skeletons)),
      Animations(std::move(Other.Animations)),
      Materials(std::move(Other.Materials))
{
}

FFBXImporter::FImportedFBXAssets& FFBXImporter::FImportedFBXAssets::operator=(FImportedFBXAssets&& Other) noexcept
{
    if (this != &Other)
    {
        for (auto* Mesh : SkeletalMeshes) delete Mesh;
        SkeletalMeshes = std::move(Other.SkeletalMeshes);
        Skeletons = std::move(Other.Skeletons);
        Animations = std::move(Other.Animations);
        Materials = std::move(Other.Materials);
    }
    return *this;
}

void FFBXImporter::Initialize()
{
    if (bInitialized) return;
    SdkManager = FbxManager::Create();
    FbxIOSettings *ios = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(ios);
    bInitialized = true;
}

bool FFBXImporter::ImportAndCacheAll(const FString& FBXFilePath, const FImportOptions& Options)
{
    FImportedFBXAssets Assets;
    if (!ImportAll(FBXFilePath, Options, Assets))
    {
        return false;
    }

    // 1. 스켈레톤 데이터 캐싱 (원본 데이터 보존용)
    for (auto* Skeleton : Assets.Skeletons)
    {
        FString SubName = SkeletalMeshPrefix::Skeleton + Skeleton->GetFName().ToString();
        FString BinPath = FPaths::BuildSubResourceCachePath(FBXFilePath, SubName);
        
        FWindowsBinWriter Writer(BinPath);
        if (Writer.IsValid())
        {
            Skeleton->Serialize(Writer);
        }
    }

    // 2. 스켈레톤별로 메시를 그룹화하여 번들(.bin) 캐싱
    for (auto* Skeleton : Assets.Skeletons)
    {
        USkeletalMesh* Container = UObjectManager::Get().CreateObject<USkeletalMesh>();
        Container->SetSkeleton(Skeleton);
        FString SkelName = Skeleton->GetFName().ToString();
        Container->SetAssetPathFileName(FBXFilePath + "_" + SkeletalMeshPrefix::Mesh + SkelName);

        for (auto* ImportedMesh : Assets.SkeletalMeshes)
        {
            if (ImportedMesh->SkeletonName == Skeleton->GetFName())
            {
                USkeletalSubMesh* Sub = UObjectManager::Get().CreateObject<USkeletalSubMesh>();
                Sub->SetSkeletalSubMeshAsset(ImportedMesh->MeshData);
                ImportedMesh->MeshData = nullptr; // 소유권 이전
                Sub->SetSkeleton(Skeleton);
                
                Container->AddSubMesh(Sub);
            }
        }

        if (!Container->GetSubMeshes().empty())
        {
            FString BinPath = FPaths::BuildSubResourceCachePath(FBXFilePath, SkeletalMeshPrefix::Mesh + SkelName);
            FWindowsBinWriter Writer(BinPath);
            if (Writer.IsValid())
            {
                Container->Serialize(Writer);
            }
        }
    }

    // 3. 애니메이션 시퀀스 캐싱
    for (auto* Anim : Assets.Animations)
    {
        FString BinPath = FPaths::BuildSubResourceCachePath(FBXFilePath, "Anim_" + Anim->GetFName().ToString());
        FWindowsBinWriter Writer(BinPath);
        if (Writer.IsValid())
        {
            Anim->Serialize(Writer);
        }
    }

    return true;
}

bool FFBXImporter::ImportAll(const FString& FBXFilePath, const FImportOptions& Options, FImportedFBXAssets& OutAssets)
{
    Initialize();
    
    // 이전 에셋 데이터 정리
    for (auto* Mesh : OutAssets.SkeletalMeshes) delete Mesh;
    OutAssets.SkeletalMeshes.clear();
    OutAssets.Skeletons.clear();
    OutAssets.Animations.clear();
    OutAssets.Materials.clear();

    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
    if(!Importer->Initialize(FBXFilePath.c_str(), -1, SdkManager->GetIOSettings()))
    {
        UE_LOG(FbxImporter, Error, "FbxImporter::Initialize() failed: %s", Importer->GetStatus().GetErrorString());
        Importer->Destroy();
        return false;
    }
    
    FbxScene* Scene = FbxScene::Create(SdkManager, "myScene");
    Importer->Import(Scene);
    Importer->Destroy();

    FbxNode* RootNode = Scene->GetRootNode();
    if(RootNode)
    {
        // 먼저 삼각형화 수행
        FbxGeometryConverter Converter = FbxGeometryConverter(SdkManager);
        Converter.Triangulate(Scene, true);

        // 그 후 엔진의 좌표계(Z-up, X-forward, Left-handed)에 맞게 씬 변환
        // DeepConvertScene은 노드 transform뿐 아니라 vertex, pose, skin cluster bind matrix까지
        // 같은 기준으로 변환하므로 Transform/TransformLink와 EvaluateGlobalTransform의 축이 어긋나지 않습니다.
        FbxAxisSystem UEAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
        UEAxisSystem.DeepConvertScene(Scene);
        
        // 1. 스켈레톤 구조 추출
        for (int i = 0; i < RootNode->GetChildCount(); i++) {
            FbxNode* ChildNode = RootNode->GetChild(i);
            FbxNodeAttribute* Attr = ChildNode->GetNodeAttribute();
            if (Attr && (Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton || Attr->GetAttributeType() == FbxNodeAttribute::eNull))
            {
                USkeleton* Skeleton = UObjectManager::Get().CreateObject<USkeleton>();
                Skeleton->SetFName(ChildNode->GetName());
                ExtractBoneNodeRecursive(ChildNode, -1, Skeleton);
                OutAssets.Skeletons.push_back(Skeleton);
            }
        }

        // 2. 메시 및 스키닝 데이터 추출
        ExtractMeshAndSkinning(RootNode, Options, OutAssets);

        // 3. 애니메이션 데이터 추출
        ExtractAnimations(Scene, OutAssets);
    }
    
    Scene->Destroy();
    return true; 
}

void FFBXImporter::ExtractBoneNodeRecursive(FbxNode* Node, int ParentIndex, USkeleton* OutSkeleton)
{
    int32 currentIndex = ParentIndex;
    FbxNodeAttribute* Attr = Node->GetNodeAttribute();
    if (Attr && (Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton || Attr->GetAttributeType() == FbxNodeAttribute::eNull))
    {
        const FMatrix LocalMatrix = ConvertFbxMatrix(Node->EvaluateLocalTransform());
        FTransform LocalTransform = GetTransformFromMatrix(LocalMatrix);
        currentIndex = OutSkeleton->AddBone(Node->GetName(), ParentIndex, LocalTransform);
        OutSkeleton->SetBoneDisplayTransform(currentIndex, LocalTransform);
        OutSkeleton->SetBoneDisplayMatrix(currentIndex, LocalMatrix);
    }

    for (int i = 0; i < Node->GetChildCount(); i++) {
        ExtractBoneNodeRecursive(Node->GetChild(i), currentIndex, OutSkeleton);
    }
}

void FFBXImporter::ExtractMeshAndSkinning(FbxNode* Node, const FImportOptions& Options, FImportedFBXAssets& OutAsset)
{
    FbxNodeAttribute* attr = Node->GetNodeAttribute();
    if (attr && attr->GetAttributeType() == FbxNodeAttribute::eMesh) {
        FbxMesh* fbxMesh = Node->GetMesh();
        
        std::unique_ptr<FSkeletalSubMesh> ExtractedMesh = ParseGeometry(fbxMesh, Options);
        if (ExtractedMesh)
        {
            BoneWeighting.assign(ExtractedMesh->Vertices.size(), TArray<FBoneWeighting>());

            int skinCount = fbxMesh->GetDeformerCount(FbxDeformer::eSkin);
            if (skinCount > 0)
            {
                FbxSkin* anySkin = (FbxSkin*)fbxMesh->GetDeformer(0, FbxDeformer::eSkin);
                if (anySkin && anySkin->GetClusterCount() > 0)
                {
                    USkeleton* OwnerSkeleton = nullptr;
                    FbxNode* currentBone = anySkin->GetCluster(0)->GetLink();
                    while (currentBone != nullptr)
                    {
                        auto it = std::find_if(OutAsset.Skeletons.begin(), OutAsset.Skeletons.end(),
                            [currentBone](USkeleton* skeleton) { return skeleton->GetFName().ToString() == currentBone->GetName(); });

                        if (it != OutAsset.Skeletons.end()) { OwnerSkeleton = *it; break; }
                        currentBone = currentBone->GetParent();
                    }

                    if (OwnerSkeleton)
                    {
                        ApplyBindPoseToSkeleton(fbxMesh, OwnerSkeleton);
                        ApplySkinBindDataToMesh(fbxMesh, OwnerSkeleton, ExtractedMesh.get());

                        for (int i = 0; i < skinCount; i++) {
                            FbxSkin* skin = (FbxSkin*)fbxMesh->GetDeformer(i, FbxDeformer::eSkin);
                            int clusterCount = skin->GetClusterCount();
                            for (int j = 0; j < clusterCount; j++) {
                                FbxCluster* cluster = skin->GetCluster(j);
                                FString boneName = cluster->GetLink()->GetName();
                                int boneIndex = OwnerSkeleton->FindBoneIndex(boneName);
                                ExtractWeights(cluster, boneIndex);
                            }
                        }
                        ApplyWeightsToSkeleton(ExtractedMesh.get());
                        
                        FImportedSkeletalMesh* NewMesh = new FImportedSkeletalMesh(
                            FName(Node->GetName()), ExtractedMesh.release(), OwnerSkeleton->GetFName());
                        OutAsset.SkeletalMeshes.push_back(NewMesh);
                    }
                }
            }
        }
    }
    
    for (int i = 0; i < Node->GetChildCount(); i++) {
        ExtractMeshAndSkinning(Node->GetChild(i), Options, OutAsset);
    }
}

std::unique_ptr<FSkeletalSubMesh> FFBXImporter::ParseGeometry(FbxMesh* InFbxMesh, const FImportOptions& Options)
{
    std::unique_ptr<FSkeletalSubMesh> Result = std::make_unique<FSkeletalSubMesh>();
    
    FbxVector4* controlPoints = InFbxMesh->GetControlPoints();
    int PolygonCount = InFbxMesh->GetPolygonCount();
    int VertexCount = 0;
    
    int TangentCount = InFbxMesh->GetElementTangentCount();
    if (TangentCount < PolygonCount)
    {
        FbxStringList UvSetNameList;
        InFbxMesh->GetUVSetNames(UvSetNameList);
        if (UvSetNameList.GetCount() > 0)
            InFbxMesh->GenerateTangentsData(UvSetNameList.GetStringAt(0));
    }
    
    FbxLayerElementArrayTemplate<FbxVector4>* TangentList = nullptr;
    InFbxMesh->GetTangents(&TangentList, 0);
    
    CtrlPointToVertexIndex.assign(InFbxMesh->GetControlPointsCount(), TArray<int>());
    for (int i = 0; i < PolygonCount; ++i) {
        uint32 TriangleIndices[3];
        for (int j = 0; j < 3; ++j) {
            int ctrlPointIndex = InFbxMesh->GetPolygonVertex(i, j);
            FVertexSkinned vertex;
            FbxVector4 pos = controlPoints[ctrlPointIndex];
            vertex.Position = FVector((float)pos[0], (float)pos[1], (float)pos[2]);

            FbxVector4 normal;
            if (InFbxMesh->GetPolygonVertexNormal(i, j, normal))
                vertex.Normal = FVector((float)normal[0], (float)normal[1], (float)normal[2]);
            
            if (TangentList)
            {
                FbxVector4 tangent = (*TangentList)[i * 3 + j];
                vertex.Tangent = FVector((float)tangent[0], (float)tangent[1], (float)tangent[2]);
            }

            FbxStringList uvSetNameList;
            InFbxMesh->GetUVSetNames(uvSetNameList);
            if (uvSetNameList.GetCount() > 0) {
                FbxVector2 uv; bool unmapped;
                if (InFbxMesh->GetPolygonVertexUV(i, j, uvSetNameList.GetStringAt(0), uv, unmapped))
                {
                    vertex.UV = FVector2((float)uv[0], 1.0f - (float)uv[1]); // UV V flip
                }
            }

            Result->Vertices.push_back(vertex);
            TriangleIndices[j] = VertexCount;
            CtrlPointToVertexIndex[ctrlPointIndex].push_back(VertexCount);
            VertexCount++;
        }

        if (Options.WindingOrder == EWindingOrder::CCW_to_CW)
        {
            Result->Indices.push_back(TriangleIndices[0]);
            Result->Indices.push_back(TriangleIndices[2]);
            Result->Indices.push_back(TriangleIndices[1]);
        }
        else
        {
            Result->Indices.push_back(TriangleIndices[0]);
            Result->Indices.push_back(TriangleIndices[1]);
            Result->Indices.push_back(TriangleIndices[2]);
        }
    }

    // 모든 정점 데이터를 포함하는 기본 섹션을 추가합니다.
    FSkeletalMeshSection DefaultSection;
    DefaultSection.MaterialIndex = 0;
    DefaultSection.MaterialSlotName = "Default";
    DefaultSection.FirstIndex = 0;
    DefaultSection.NumTriangles = (uint32)PolygonCount;
    Result->Sections.push_back(DefaultSection);

    return Result;
}

FTransform FFBXImporter::GetTransformFromNode(FbxNode* Node)
{
    if (!Node)
    {
        return FTransform();
    }

    return GetTransformFromMatrix(ConvertFbxMatrix(Node->EvaluateLocalTransform()));
}

FTransform FFBXImporter::GetTransformFromMatrix(const FMatrix& Matrix)
{
    FTransform Result;
    Result.Location = Matrix.GetLocation();
    Result.Scale = Matrix.GetScale();

    FMatrix RotationMatrix = Matrix;
    if (Result.Scale.X > 1e-6f)
    {
        RotationMatrix.M[0][0] /= Result.Scale.X;
        RotationMatrix.M[0][1] /= Result.Scale.X;
        RotationMatrix.M[0][2] /= Result.Scale.X;
    }
    if (Result.Scale.Y > 1e-6f)
    {
        RotationMatrix.M[1][0] /= Result.Scale.Y;
        RotationMatrix.M[1][1] /= Result.Scale.Y;
        RotationMatrix.M[1][2] /= Result.Scale.Y;
    }
    if (Result.Scale.Z > 1e-6f)
    {
        RotationMatrix.M[2][0] /= Result.Scale.Z;
        RotationMatrix.M[2][1] /= Result.Scale.Z;
        RotationMatrix.M[2][2] /= Result.Scale.Z;
    }

    RotationMatrix.M[3][0] = 0.0f;
    RotationMatrix.M[3][1] = 0.0f;
    RotationMatrix.M[3][2] = 0.0f;
    RotationMatrix.M[3][3] = 1.0f;
    Result.Rotation = RotationMatrix.ToQuat();
    return Result;
}

FMatrix FFBXImporter::ConvertFbxMatrix(const FbxAMatrix& Matrix)
{
    return FMatrix(
        static_cast<float>(Matrix.Get(0, 0)), static_cast<float>(Matrix.Get(0, 1)), static_cast<float>(Matrix.Get(0, 2)), static_cast<float>(Matrix.Get(0, 3)),
        static_cast<float>(Matrix.Get(1, 0)), static_cast<float>(Matrix.Get(1, 1)), static_cast<float>(Matrix.Get(1, 2)), static_cast<float>(Matrix.Get(1, 3)),
        static_cast<float>(Matrix.Get(2, 0)), static_cast<float>(Matrix.Get(2, 1)), static_cast<float>(Matrix.Get(2, 2)), static_cast<float>(Matrix.Get(2, 3)),
        static_cast<float>(Matrix.Get(3, 0)), static_cast<float>(Matrix.Get(3, 1)), static_cast<float>(Matrix.Get(3, 2)), static_cast<float>(Matrix.Get(3, 3)));
}

FMatrix FFBXImporter::ConvertFbxMatrix(const FbxMatrix& Matrix)
{
    return FMatrix(
        static_cast<float>(Matrix.Get(0, 0)), static_cast<float>(Matrix.Get(0, 1)), static_cast<float>(Matrix.Get(0, 2)), static_cast<float>(Matrix.Get(0, 3)),
        static_cast<float>(Matrix.Get(1, 0)), static_cast<float>(Matrix.Get(1, 1)), static_cast<float>(Matrix.Get(1, 2)), static_cast<float>(Matrix.Get(1, 3)),
        static_cast<float>(Matrix.Get(2, 0)), static_cast<float>(Matrix.Get(2, 1)), static_cast<float>(Matrix.Get(2, 2)), static_cast<float>(Matrix.Get(2, 3)),
        static_cast<float>(Matrix.Get(3, 0)), static_cast<float>(Matrix.Get(3, 1)), static_cast<float>(Matrix.Get(3, 2)), static_cast<float>(Matrix.Get(3, 3)));
}

void FFBXImporter::ApplyBindPoseToSkeleton(FbxMesh* InFbxMesh, USkeleton* InSkeleton)
{
    if (!InFbxMesh || !InSkeleton)
    {
        return;
    }

    std::unordered_map<FbxNode*, FMatrix> GlobalBindMatrices;

    FbxScene* Scene = InFbxMesh->GetScene();
    if (Scene)
    {
        for (int PoseIndex = 0; PoseIndex < Scene->GetPoseCount(); ++PoseIndex)
        {
            FbxPose* Pose = Scene->GetPose(PoseIndex);
            if (!Pose || !Pose->IsBindPose())
            {
                continue;
            }

            for (int NodeIndex = 0; NodeIndex < Pose->GetCount(); ++NodeIndex)
            {
                FbxNode* PoseNode = Pose->GetNode(NodeIndex);
                if (!PoseNode)
                {
                    continue;
                }

                const FbxMatrix& SourceMatrix = Pose->GetMatrix(NodeIndex);
                GlobalBindMatrices[PoseNode] = ConvertFbxMatrix(SourceMatrix);
            }
        }
    }

    const int SkinCount = InFbxMesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
    {
        FbxSkin* Skin = static_cast<FbxSkin*>(InFbxMesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
        if (!Skin)
        {
            continue;
        }

        const int ClusterCount = Skin->GetClusterCount();
        for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            if (!Cluster || !Cluster->GetLink())
            {
                continue;
            }

            FbxAMatrix LinkBindMatrix;
            Cluster->GetTransformLinkMatrix(LinkBindMatrix);
            GlobalBindMatrices[Cluster->GetLink()] = ConvertFbxMatrix(LinkBindMatrix);
        }
    }

    const TArray<FBoneInfo>& Bones = InSkeleton->GetBones();
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        FbxNode* BoneNode = nullptr;
        if (Scene && Scene->GetRootNode())
        {
            BoneNode = Scene->GetRootNode()->FindChild(Bone.Name.ToString().c_str(), true);
        }

        auto BindIt = BoneNode ? GlobalBindMatrices.find(BoneNode) : GlobalBindMatrices.end();
        if (BindIt == GlobalBindMatrices.end())
        {
            continue;
        }

        FMatrix LocalBindMatrix = BindIt->second;
        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(Bones.size()))
        {
            FbxNode* ParentNode = nullptr;
            if (Scene && Scene->GetRootNode())
            {
                ParentNode = Scene->GetRootNode()->FindChild(Bones[Bone.ParentIndex].Name.ToString().c_str(), true);
            }

            auto ParentBindIt = ParentNode ? GlobalBindMatrices.find(ParentNode) : GlobalBindMatrices.end();
            if (ParentBindIt != GlobalBindMatrices.end())
            {
                LocalBindMatrix = BindIt->second * ParentBindIt->second.GetInverse();
            }
        }

        InSkeleton->SetBoneReferenceTransform(BoneIndex, GetTransformFromMatrix(LocalBindMatrix));
        InSkeleton->SetBoneReferenceMatrix(BoneIndex, LocalBindMatrix);
    }
}

void FFBXImporter::ApplySkinBindDataToMesh(FbxMesh* InFbxMesh, USkeleton* InSkeleton, FSkeletalSubMesh* InMesh)
{
    if (!InFbxMesh || !InSkeleton || !InMesh)
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = InSkeleton->GetBones();
    InMesh->InverseBindPoseMatrices.clear();
    InMesh->InverseBindPoseMatrices.resize(Bones.size(), FMatrix::Identity);
    InMesh->BoneBindGlobalMatrices.clear();
    InMesh->BoneBindGlobalMatrices.resize(Bones.size(), FMatrix::Identity);

    bool bHasMeshBindScale = false;
    InMesh->MeshBindInverseScale = FVector(1.0f);

    const int SkinCount = InFbxMesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
    {
        FbxSkin* Skin = static_cast<FbxSkin*>(InFbxMesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
        if (!Skin)
        {
            continue;
        }

        const int ClusterCount = Skin->GetClusterCount();
        for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            if (!Cluster || !Cluster->GetLink())
            {
                continue;
            }

            const int32 BoneIndex = InSkeleton->FindBoneIndex(Cluster->GetLink()->GetName());
            if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(InMesh->InverseBindPoseMatrices.size()))
            {
                continue;
            }

            FbxAMatrix MeshBindMatrix;
            FbxAMatrix BoneBindMatrix;
            Cluster->GetTransformMatrix(MeshBindMatrix);
            Cluster->GetTransformLinkMatrix(BoneBindMatrix);

            const FMatrix MeshBindGlobal = ConvertFbxMatrix(MeshBindMatrix);
            const FMatrix BoneBindGlobal = ConvertFbxMatrix(BoneBindMatrix);

            if (!bHasMeshBindScale)
            {
                InMesh->MeshBindInverseScale = GetSafeInverseScale(MeshBindGlobal.GetScale());
                bHasMeshBindScale = true;
            }

            InMesh->InverseBindPoseMatrices[BoneIndex] = MeshBindGlobal * BoneBindGlobal.GetInverse();
            InMesh->BoneBindGlobalMatrices[BoneIndex] = BoneBindGlobal;
        }
    }
}

void FFBXImporter::ExtractWeights(FbxCluster* InCluster, int InBoneIndex)
{
    if (InBoneIndex < 0)
    {
        return;
    }

    int CtrlPointCount = InCluster->GetControlPointIndicesCount();
    int* CtrlPoints = InCluster->GetControlPointIndices();
    double* CtrlPointWeights = InCluster->GetControlPointWeights();
    for (int i = 0; i < CtrlPointCount; i++)
    {
        int CtrlPointIndex = CtrlPoints[i];
        float Weight = (float)CtrlPointWeights[i];
        for (int VertexIndex : CtrlPointToVertexIndex[CtrlPointIndex])
            BoneWeighting[VertexIndex].push_back({(uint16)InBoneIndex, Weight});
    }
}

void FFBXImporter::ApplyWeightsToSkeleton(FSkeletalSubMesh* InMesh)
{
    for (size_t i = 0; i < InMesh->Vertices.size(); i++)
    {
        auto& Weighting = BoneWeighting[i];
        std::ranges::sort(Weighting, [](const FBoneWeighting& A, const FBoneWeighting& B){ return A.second > B.second; });
        for (int b = 0; b < std::min(8, (int)Weighting.size()); b++)
        {
            InMesh->Vertices[i].BoneIndices[b] = Weighting[b].first;
            InMesh->Vertices[i].BoneWeights[b] = Weighting[b].second;
        }
    }
}

// 이하 AI가 작성하고 아직 검증되지 않은 코드입니다. 
void FFBXImporter::ExtractAnimations(FbxScene* Scene, FImportedFBXAssets& OutAssets)
{
    int AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    if (AnimStackCount == 0 || OutAssets.Skeletons.empty()) return;

    USkeleton* TargetSkeleton = OutAssets.Skeletons[0];
    FbxNode* RootNode = Scene->GetRootNode();

    for (int i = 0; i < AnimStackCount; i++)
    {
        FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(i);
        Scene->SetCurrentAnimationStack(AnimStack);

        FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
        FbxTime Start = TimeSpan.GetStart();
        FbxTime End = TimeSpan.GetStop();
        FbxTime Duration = End - Start;

        UAnimationSequence* AnimSeq = UObjectManager::Get().CreateObject<UAnimationSequence>();
        AnimSeq->SetFName(AnimStack->GetName());
        AnimSeq->SetSkeleton(TargetSkeleton);
        
        float FPS = 30.0f; 
        FbxTime FrameStep;
        FrameStep.SetTime(0, 0, 0, 1, 0, FbxTime::eFrames30);
        
        int32 NumFrames = (int32)(Duration.Get() / FrameStep.Get()) + 1;
        if (NumFrames <= 0) continue;

        AnimSeq->SetNumFrames(NumFrames);
        AnimSeq->SetSequenceLength((float)Duration.GetSecondDouble());
        AnimSeq->SetFPS(FPS);

        const TArray<FBoneInfo>& Bones = TargetSkeleton->GetBones();
        AnimSeq->GetTracks().resize(Bones.size());

        for (int32 Frame = 0; Frame < NumFrames; Frame++)
        {
            FbxTime CurrentTime = Start + FrameStep * Frame;
            
            for (int32 BoneIdx = 0; BoneIdx < (int32)Bones.size(); BoneIdx++)
            {
                const FBoneInfo& BoneInfo = Bones[BoneIdx];
                FbxNode* BoneNode = RootNode->FindChild(BoneInfo.Name.ToString().c_str(), true);
                
                if (BoneNode)
                {
                    FbxAMatrix LocalMatrix = BoneNode->EvaluateLocalTransform(CurrentTime);
                    FbxVector4 T = LocalMatrix.GetT();
                    FbxQuaternion Q = LocalMatrix.GetQ();
                    FbxVector4 S = LocalMatrix.GetS();

                    AnimSeq->GetTracks()[BoneIdx].PosKeys.push_back(FVector((float)T[0], (float)T[1], (float)T[2]));
                    AnimSeq->GetTracks()[BoneIdx].RotKeys.push_back(FQuat((float)Q[0], (float)Q[1], (float)Q[2], (float)Q[3]));
                    AnimSeq->GetTracks()[BoneIdx].ScaleKeys.push_back(FVector((float)S[0], (float)S[1], (float)S[2]));
                }
                else
                {
                    AnimSeq->GetTracks()[BoneIdx].PosKeys.push_back(BoneInfo.ReferenceTransform.Location);
                    AnimSeq->GetTracks()[BoneIdx].RotKeys.push_back(BoneInfo.ReferenceTransform.Rotation);
                    AnimSeq->GetTracks()[BoneIdx].ScaleKeys.push_back(BoneInfo.ReferenceTransform.Scale);
                }
            }
        }
        OutAssets.Animations.push_back(AnimSeq);
    }
}
