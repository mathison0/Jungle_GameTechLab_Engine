#include "Mesh/FBXImporter.h"
#include "Mesh/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Core/Logging/LogMacros.h"
#include "Object/Object.h"
#include <filesystem>
#include <algorithm>
#include <ranges>
#include <fstream>

namespace
{
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
    for (auto* Mesh : SkeletalMeshes)
    {
        delete Mesh;
    }
    SkeletalMeshes.clear();
}

FFBXImporter::FImportedFBXAssets::FImportedFBXAssets(FImportedFBXAssets&& Other) noexcept
    : SkeletalMeshes(std::move(Other.SkeletalMeshes)),
      Skeletons(std::move(Other.Skeletons)),
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

    // 1. Skeletal Meshes 캐싱
    for (auto* ImportedMesh : Assets.SkeletalMeshes)
    {
        // 논리적 식별자 부여 (예: Asset/Models/Hero.fbx_Mesh_Body)
        ImportedMesh->MeshData->PathFileName = FBXFilePath + "_Mesh_" + ImportedMesh->Name.ToString();

        FString BinPath = FPaths::BuildSubResourceCachePath(FBXFilePath, "Mesh_" + ImportedMesh->Name.ToString());
        
        USkeletalSubMesh* TempMesh = UObjectManager::Get().CreateObject<USkeletalSubMesh>();
        TempMesh->SetSkeletalSubMeshAsset(ImportedMesh->MeshData);
        ImportedMesh->MeshData = nullptr; 
        
        auto it = std::find_if(Assets.Skeletons.begin(), Assets.Skeletons.end(), 
            [&](USkeleton* S) { return S->GetFName() == ImportedMesh->SkeletonName; });
        if (it != Assets.Skeletons.end())
        {
            TempMesh->SetSkeleton(*it);
        }
        
        FWindowsBinWriter Writer(BinPath);
        if (Writer.IsValid())
        {
            TempMesh->Serialize(Writer);
        }
    }

    // 2. Skeletons 캐싱
    for (auto* Skeleton : Assets.Skeletons)
    {
        // "Skel_" 접두사 추가
        FString SubName = "Skel_" + Skeleton->GetFName().ToString();
        FString BinPath = FPaths::BuildSubResourceCachePath(FBXFilePath, SubName);
        
        FWindowsBinWriter Writer(BinPath);
        if (Writer.IsValid())
        {
            Skeleton->Serialize(Writer);
        }
    }

    return true;
}

bool FFBXImporter::ImportAll(const FString& FBXFilePath, const FImportOptions& Options, FImportedFBXAssets& OutAssets)
{
    Initialize();
    
    for (auto* Mesh : OutAssets.SkeletalMeshes) delete Mesh;
    OutAssets.SkeletalMeshes.clear();
    OutAssets.Skeletons.clear();
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
        FbxGeometryConverter Converter = FbxGeometryConverter(SdkManager);
        Converter.Triangulate(Scene, true);
        
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
        ExtractMeshAndSkinning(RootNode, OutAssets);
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
        FTransform LocalTransform = GetTransformFromNode(Node);
        currentIndex = OutSkeleton->AddBone(Node->GetName(), ParentIndex, LocalTransform);
    }

    for (int i = 0; i < Node->GetChildCount(); i++) {
        ExtractBoneNodeRecursive(Node->GetChild(i), currentIndex, OutSkeleton);
    }
}

void FFBXImporter::ExtractMeshAndSkinning(FbxNode* Node, FImportedFBXAssets& OutAsset)
{
    FbxNodeAttribute* attr = Node->GetNodeAttribute();
    if (attr && attr->GetAttributeType() == FbxNodeAttribute::eMesh) {
        FbxMesh* fbxMesh = Node->GetMesh();
        
        std::unique_ptr<FSkeletalSubMesh> ExtractedMesh = ParseGeometry(fbxMesh);
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
        ExtractMeshAndSkinning(Node->GetChild(i), OutAsset);
    }
}

std::unique_ptr<FSkeletalSubMesh> FFBXImporter::ParseGeometry(FbxMesh* InFbxMesh)
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
                    vertex.UV = FVector2((float)uv[0], (float)uv[1]);
            }

            Result->Vertices.push_back(vertex);
            Result->Indices.push_back(VertexCount);
            CtrlPointToVertexIndex[ctrlPointIndex].push_back(VertexCount);
            VertexCount++;
        }
    }
    return Result;
}

FTransform FFBXImporter::GetTransformFromNode(FbxNode* Node)
{
    FTransform Result;
    FbxVector4 LclT = Node->LclTranslation.Get();
    Result.Location = FVector((float)LclT[0], (float)LclT[1], (float)LclT[2]);
    FbxVector4 LclR = Node->LclRotation.Get();
    Result.Rotation = FQuat::FromRotator(FRotator((float)LclR[0], (float)LclR[1], (float)LclR[2]));
    FbxVector4 LclS = Node->LclScaling.Get();
    Result.Scale = FVector((float)LclS[0], (float)LclS[1], (float)LclS[2]);
    return Result;
}

void FFBXImporter::ExtractWeights(FbxCluster* InCluster, int InBoneIndex)
{
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

void FFBXImporter::ExtractAnimations(FbxScene* scene) {}