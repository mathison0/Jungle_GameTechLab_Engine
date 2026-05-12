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

    const char* GetAxisName(FbxAxisSystem::EUpVector UpVector)
    {
        switch (UpVector)
        {
        case FbxAxisSystem::eXAxis: return "X";
        case FbxAxisSystem::eYAxis: return "Y";
        case FbxAxisSystem::eZAxis: return "Z";
        default: return "Unknown";
        }
    }

    const char* GetFrontAxisName(FbxAxisSystem::EUpVector UpVector, FbxAxisSystem::EFrontVector FrontVector)
    {
        switch (UpVector)
        {
        case FbxAxisSystem::eXAxis:
            return FrontVector == FbxAxisSystem::eParityEven ? "Y" : "Z";
        case FbxAxisSystem::eYAxis:
            return FrontVector == FbxAxisSystem::eParityEven ? "X" : "Z";
        case FbxAxisSystem::eZAxis:
            return FrontVector == FbxAxisSystem::eParityEven ? "X" : "Y";
        default:
            return "Unknown";
        }
    }

    const char* GetCoordSystemName(FbxAxisSystem::ECoordSystem CoordSystem)
    {
        switch (CoordSystem)
        {
        case FbxAxisSystem::eRightHanded: return "RightHanded";
        case FbxAxisSystem::eLeftHanded: return "LeftHanded";
        default: return "Unknown";
        }
    }

    void LogAxisSystem(const char* Label, const FbxAxisSystem& AxisSystem)
    {
        int UpSign = 1;
        int FrontSign = 1;
        const FbxAxisSystem::EUpVector UpVector = AxisSystem.GetUpVector(UpSign);
        const FbxAxisSystem::EFrontVector FrontVector = AxisSystem.GetFrontVector(FrontSign);
        const FbxAxisSystem::ECoordSystem CoordSystem = AxisSystem.GetCoorSystem();

        UE_LOG(FBXImporter, Warning,
            "[FBX Axis] %s Up=%s%s Front=%s%s Coord=%s (UpEnum=%d FrontEnum=%d UpSign=%d FrontSign=%d)",
            Label,
            UpSign >= 0 ? "+" : "-",
            GetAxisName(UpVector),
            FrontSign >= 0 ? "+" : "-",
            GetFrontAxisName(UpVector, FrontVector),
            GetCoordSystemName(CoordSystem),
            static_cast<int>(UpVector),
            static_cast<int>(FrontVector),
            UpSign,
            FrontSign);
    }

    // 테스트용 함수
    void PrintNode(FbxNode* pNode, int depth) {
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

        FString tab = "";
        for (int i=0; i<depth; i++)
        {
            tab = tab + "  ";
        }
        
        UE_LOG(FBXImporter, Warning, 
            "%s <node name='%s' attribute = '%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
            tab.c_str(),
            nodeName, attrStr.c_str(),
            translation[0], translation[1], translation[2],
            rotation[0], rotation[1], rotation[2],
            scaling[0], scaling[1], scaling[2]
            );

        for(int j = 0; j < pNode->GetChildCount(); j++)
            PrintNode(pNode->GetChild(j), depth + 1);
    }
} // namespace

bool FFBXImporter::bInitialized = false;
FbxManager* FFBXImporter::SdkManager = nullptr;
TArray<TArray<int>> FFBXImporter::CtrlPointToVertexIndex;
TArray<TArray<FFBXImporter::FBoneWeighting>> FFBXImporter::BoneWeighting;

namespace
{
    std::unordered_map<const USkeleton*, FVector> GSkeletonMeshBindInverseScales;

    FString BuildValidMaterialSlotName(const FString& InName, int32 FallbackIndex)
    {
        if (!InName.empty())
        {
            return InName;
        }

        return "Material_" + std::to_string(FallbackIndex);
    }

    int32 GetFbxPolygonMaterialIndex(FbxNode* Node, FbxMesh* Mesh, int32 PolygonIndex)
    {
        if (!Node || !Mesh)
        {
            return 0;
        }

        const int32 MaterialCount = Node->GetMaterialCount();
        if (MaterialCount <= 0)
        {
            return 0;
        }

        FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();
        if (!MaterialElement)
        {
            return 0;
        }

        int32 MaterialIndex = 0;
        switch (MaterialElement->GetMappingMode())
        {
        case FbxGeometryElement::eAllSame:
            if (MaterialElement->GetIndexArray().GetCount() > 0)
            {
                MaterialIndex = MaterialElement->GetIndexArray().GetAt(0);
            }
            break;
        case FbxGeometryElement::eByPolygon:
            if (PolygonIndex >= 0 && PolygonIndex < MaterialElement->GetIndexArray().GetCount())
            {
                MaterialIndex = MaterialElement->GetIndexArray().GetAt(PolygonIndex);
            }
            break;
        default:
            MaterialIndex = 0;
            break;
        }

        return std::clamp(MaterialIndex, 0, MaterialCount - 1);
    }

}

FFBXImporter::FImportedSkeletalMesh::FImportedSkeletalMesh(FName InName, FSkeletalSubMesh* InMeshData, FName InSkeletonName, TArray<FStaticMaterial>&& InMaterials)
    : Name(InName), MeshData(InMeshData), SkeletonName(InSkeletonName), Materials(std::move(InMaterials))
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
                Sub->SetStaticMaterials(std::move(ImportedMesh->Materials));
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
    GSkeletonMeshBindInverseScales.clear();

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
        // DeepConvertScene 이후의 node/evaluated transform을 기준으로 bind matrix를 다시 구성합니다.
        // FBX의 pose/skin cluster bind matrix는 변환 후에도 source axis 기준 값이 남는 경우가 있어
        // 그대로 쓰면 bone은 변환됐는데 mesh bind만 원래 축에 남는 문제가 생깁니다.
        FbxAxisSystem UEAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
        LogAxisSystem("Source metadata before convert", Scene->GetGlobalSettings().GetAxisSystem());
        LogAxisSystem("Importer target", UEAxisSystem);
        UEAxisSystem.DeepConvertScene(Scene);
        LogAxisSystem("Scene metadata after convert", Scene->GetGlobalSettings().GetAxisSystem());

        //// 단위계도 엔진 기준(미터)으로 정규화. cm 기반 FBX(블렌더/MMD 등)와 m 기반 FBX 모두 동일한 스케일에서 처리하기 위함.
        //if (Scene->GetGlobalSettings().GetSystemUnit() != FbxSystemUnit::m)
        //{
        //    FbxSystemUnit::m.ConvertScene(Scene);
        //}
        
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
    auto ImportPass = [&](auto&& Self, FbxNode* CurrentNode, bool bFallbackPass) -> void
    {
        if (!CurrentNode)
        {
            return;
        }

        FbxNodeAttribute* Attr = CurrentNode->GetNodeAttribute();
        if (Attr && Attr->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            FbxMesh* fbxMesh = CurrentNode->GetMesh();
            FbxSkin* AnySkin = nullptr;
            const int SkinCount = fbxMesh ? fbxMesh->GetDeformerCount(FbxDeformer::eSkin) : 0;
            if (SkinCount > 0)
            {
                AnySkin = static_cast<FbxSkin*>(fbxMesh->GetDeformer(0, FbxDeformer::eSkin));
            }

            const bool bHasSkinClusters = AnySkin && AnySkin->GetClusterCount() > 0;
            if (bFallbackPass != !bHasSkinClusters)
            {
                for (int ChildIndex = 0; ChildIndex < CurrentNode->GetChildCount(); ++ChildIndex)
                {
                    Self(Self, CurrentNode->GetChild(ChildIndex), bFallbackPass);
                }
                return;
            }

            TArray<FStaticMaterial> ExtractedMaterials;
            std::unique_ptr<FSkeletalSubMesh> ExtractedMesh = ParseGeometry(CurrentNode, fbxMesh, Options, ExtractedMaterials);
            if (ExtractedMesh)
            {
                BoneWeighting.assign(ExtractedMesh->Vertices.size(), TArray<FBoneWeighting>());

                if (!bFallbackPass)
                {
                    USkeleton* OwnerSkeleton = FindOwnerSkeletonByBoneNode(
                        AnySkin->GetCluster(0)->GetLink(),
                        OutAsset.Skeletons);
                    if (OwnerSkeleton)
                    {
                        ApplyBindPoseToSkeleton(fbxMesh, OwnerSkeleton);
                        ApplySkinBindDataToMesh(fbxMesh, OwnerSkeleton, ExtractedMesh.get());

                        for (int i = 0; i < SkinCount; i++) {
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
                            FName(CurrentNode->GetName()), ExtractedMesh.release(), OwnerSkeleton->GetFName(), std::move(ExtractedMaterials));
                        OutAsset.SkeletalMeshes.push_back(NewMesh);
                    }
                }
                else
                {
                    int32 ParentBoneIndex = -1;
                    FbxNode* ParentBoneNode = nullptr;
                    USkeleton* OwnerSkeleton = FindOwnerSkeletonByBoneNode(
                        CurrentNode->GetParent(),
                        OutAsset.Skeletons,
                        &ParentBoneIndex,
                        &ParentBoneNode);

                    if (!OwnerSkeleton && OutAsset.Skeletons.size() == 1)
                    {
                        OwnerSkeleton = OutAsset.Skeletons[0];
                        const TArray<FBoneInfo>& Bones = OwnerSkeleton->GetBones();
                        if (!Bones.empty())
                        {
                            ParentBoneIndex = 0;
                            FbxScene* Scene = fbxMesh ? fbxMesh->GetScene() : nullptr;
                            ParentBoneNode = Scene && Scene->GetRootNode()
                                ? Scene->GetRootNode()->FindChild(Bones[0].Name.ToString().c_str(), true)
                                : nullptr;
                        }
                    }

                    if (OwnerSkeleton && ApplyRigidParentWeightFallback(CurrentNode, OwnerSkeleton, ParentBoneIndex, ParentBoneNode, ExtractedMesh.get()))
                    {
                        ApplyWeightsToSkeleton(ExtractedMesh.get());

                        FImportedSkeletalMesh* NewMesh = new FImportedSkeletalMesh(
                            FName(CurrentNode->GetName()), ExtractedMesh.release(), OwnerSkeleton->GetFName(), std::move(ExtractedMaterials));
                        OutAsset.SkeletalMeshes.push_back(NewMesh);
                    }
                }
            }
        }

        for (int ChildIndex = 0; ChildIndex < CurrentNode->GetChildCount(); ++ChildIndex)
        {
            Self(Self, CurrentNode->GetChild(ChildIndex), bFallbackPass);
        }
    };

    ImportPass(ImportPass, Node, false);
    ImportPass(ImportPass, Node, true);
}

std::unique_ptr<FSkeletalSubMesh> FFBXImporter::ParseGeometry(FbxNode* InNode, FbxMesh* InFbxMesh, const FImportOptions& Options, TArray<FStaticMaterial>& OutMaterials)
{
    std::unique_ptr<FSkeletalSubMesh> Result = std::make_unique<FSkeletalSubMesh>();
    
    const FMatrix GeometryMatrix = GetGeometryMatrix(InNode);
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
    
    struct FPendingSectionBuild
    {
        FString MaterialSlotName;
        TArray<uint32> Indices;
    };

    TArray<FPendingSectionBuild> PendingSections;
    TArray<int32> FbxMaterialIndexToSectionIndex;
    const int32 NodeMaterialCount = InNode ? InNode->GetMaterialCount() : 0;
    if (NodeMaterialCount > 0)
    {
        FbxMaterialIndexToSectionIndex.assign(NodeMaterialCount, -1);
    }

    CtrlPointToVertexIndex.assign(InFbxMesh->GetControlPointsCount(), TArray<int>());
    for (int i = 0; i < PolygonCount; ++i) {
        uint32 TriangleIndices[3];
        for (int j = 0; j < 3; ++j) {
            int ctrlPointIndex = InFbxMesh->GetPolygonVertex(i, j);
            FVertexSkinned vertex;
            FbxVector4 pos = controlPoints[ctrlPointIndex];
            vertex.Position = GeometryMatrix.TransformPositionWithW(FVector((float)pos[0], (float)pos[1], (float)pos[2]));

            FbxVector4 normal;
            if (InFbxMesh->GetPolygonVertexNormal(i, j, normal))
            {
                vertex.Normal = GeometryMatrix.TransformVector(FVector((float)normal[0], (float)normal[1], (float)normal[2]));
                vertex.Normal.Normalize();
            }
            
            if (TangentList)
            {
                FbxVector4 tangent = (*TangentList)[i * 3 + j];
                FVector TangentVector = GeometryMatrix.TransformVector(FVector((float)tangent[0], (float)tangent[1], (float)tangent[2]));
                TangentVector.Normalize();
                vertex.Tangent = TangentVector;
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
            std::swap(TriangleIndices[1], TriangleIndices[2]);
        }

        int32 SectionIndex = 0;
        if (NodeMaterialCount > 0)
        {
            const int32 FbxMaterialIndex = GetFbxPolygonMaterialIndex(InNode, InFbxMesh, i);
            SectionIndex = FbxMaterialIndexToSectionIndex[FbxMaterialIndex];
            if (SectionIndex == -1)
            {
                FPendingSectionBuild NewSection;
                FbxSurfaceMaterial* FbxMaterial = InNode->GetMaterial(FbxMaterialIndex);
                NewSection.MaterialSlotName = BuildValidMaterialSlotName(
                    FbxMaterial ? FString(FbxMaterial->GetName()) : FString(),
                    FbxMaterialIndex);
                PendingSections.push_back(std::move(NewSection));
                SectionIndex = static_cast<int32>(PendingSections.size()) - 1;
                FbxMaterialIndexToSectionIndex[FbxMaterialIndex] = SectionIndex;

                FStaticMaterial NewMaterial;
                NewMaterial.MaterialInterface = nullptr;
                NewMaterial.MaterialSlotName = PendingSections[SectionIndex].MaterialSlotName;
                OutMaterials.push_back(std::move(NewMaterial));
            }
        }
        else if (PendingSections.empty())
        {
            FPendingSectionBuild DefaultSectionBuild;
            DefaultSectionBuild.MaterialSlotName = "Default";
            PendingSections.push_back(std::move(DefaultSectionBuild));

            FStaticMaterial DefaultMaterial;
            DefaultMaterial.MaterialInterface = nullptr;
            DefaultMaterial.MaterialSlotName = "Default";
            OutMaterials.push_back(std::move(DefaultMaterial));
        }

        PendingSections[SectionIndex].Indices.push_back(TriangleIndices[0]);
        PendingSections[SectionIndex].Indices.push_back(TriangleIndices[1]);
        PendingSections[SectionIndex].Indices.push_back(TriangleIndices[2]);
    }

    Result->Indices.clear();
    Result->Indices.reserve(static_cast<size_t>(PolygonCount) * 3);
    for (int32 SectionIndex = 0; SectionIndex < static_cast<int32>(PendingSections.size()); ++SectionIndex)
    {
        const FPendingSectionBuild& PendingSection = PendingSections[SectionIndex];
        if (PendingSection.Indices.empty())
        {
            continue;
        }

        FSkeletalMeshSection NewSection;
        NewSection.MaterialIndex = SectionIndex;
        NewSection.MaterialSlotName = PendingSection.MaterialSlotName;
        NewSection.FirstIndex = static_cast<uint32>(Result->Indices.size());
        NewSection.NumTriangles = static_cast<uint32>(PendingSection.Indices.size() / 3);
        Result->Sections.push_back(NewSection);
        Result->Indices.insert(Result->Indices.end(), PendingSection.Indices.begin(), PendingSection.Indices.end());
    }

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

FMatrix FFBXImporter::GetGeometryMatrix(FbxNode* Node)
{
    if (!Node)
    {
        return FMatrix::Identity;
    }

    FbxAMatrix GeometryMatrix;
    GeometryMatrix.SetT(Node->GetGeometricTranslation(FbxNode::eSourcePivot));
    GeometryMatrix.SetR(Node->GetGeometricRotation(FbxNode::eSourcePivot));
    GeometryMatrix.SetS(Node->GetGeometricScaling(FbxNode::eSourcePivot));
    return ConvertFbxMatrix(GeometryMatrix);
}

FVector FFBXImporter::GetSkeletonMeshBindInverseScale(USkeleton* Skeleton, const FMatrix& FallbackMeshBindGlobal)
{
    auto It = GSkeletonMeshBindInverseScales.find(Skeleton);
    if (It != GSkeletonMeshBindInverseScales.end())
    {
        return It->second;
    }

    return GetSafeInverseScale(FallbackMeshBindGlobal.GetScale());
}

void FFBXImporter::ApplyBindPoseToSkeleton(FbxMesh* InFbxMesh, USkeleton* InSkeleton)
{
    if (!InFbxMesh || !InSkeleton)
    {
        return;
    }

    FbxScene* Scene = InFbxMesh->GetScene();
    FbxNode* RootNode = Scene ? Scene->GetRootNode() : nullptr;
    if (!RootNode)
    {
        return;
    }

    std::unordered_map<FbxNode*, FMatrix> GlobalBindMatrices;
    const TArray<FBoneInfo>& Bones = InSkeleton->GetBones();
    for (const FBoneInfo& Bone : Bones)
    {
        FbxNode* BoneNode = RootNode->FindChild(Bone.Name.ToString().c_str(), true);
        if (BoneNode)
        {
            GlobalBindMatrices[BoneNode] = ConvertFbxMatrix(BoneNode->EvaluateGlobalTransform());
        }
    }

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        FbxNode* BoneNode = RootNode->FindChild(Bone.Name.ToString().c_str(), true);

        auto BindIt = BoneNode ? GlobalBindMatrices.find(BoneNode) : GlobalBindMatrices.end();
        if (BindIt == GlobalBindMatrices.end())
        {
            continue;
        }

        FMatrix LocalBindMatrix = BindIt->second;
        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(Bones.size()))
        {
            FbxNode* ParentNode = RootNode->FindChild(Bones[Bone.ParentIndex].Name.ToString().c_str(), true);

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
    FbxNode* MeshNode = InFbxMesh->GetNode();
    const FMatrix MeshBindGlobal = MeshNode
        ? ConvertFbxMatrix(MeshNode->EvaluateGlobalTransform())
        : FMatrix::Identity;

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

            const FMatrix BoneBindGlobal = ConvertFbxMatrix(Cluster->GetLink()->EvaluateGlobalTransform());

            if (!bHasMeshBindScale)
            {
                InMesh->MeshBindInverseScale = GetSafeInverseScale(MeshBindGlobal.GetScale());
                GSkeletonMeshBindInverseScales.emplace(InSkeleton, InMesh->MeshBindInverseScale);
                bHasMeshBindScale = true;
            }

            InMesh->InverseBindPoseMatrices[BoneIndex] = MeshBindGlobal * BoneBindGlobal.GetInverse();
            InMesh->BoneBindGlobalMatrices[BoneIndex] = BoneBindGlobal;
        }
    }
}

USkeleton* FFBXImporter::FindOwnerSkeletonByBoneNode(
    FbxNode* BoneNode,
    const TArray<USkeleton*>& Skeletons,
    int32* OutBoneIndex,
    FbxNode** OutMatchedBoneNode)
{
    for (FbxNode* CurrentNode = BoneNode; CurrentNode != nullptr; CurrentNode = CurrentNode->GetParent())
    {
        for (USkeleton* Skeleton : Skeletons)
        {
            if (!Skeleton)
            {
                continue;
            }

            const int32 BoneIndex = Skeleton->FindBoneIndex(CurrentNode->GetName());
            if (BoneIndex < 0)
            {
                continue;
            }

            if (OutBoneIndex)
            {
                *OutBoneIndex = BoneIndex;
            }

            if (OutMatchedBoneNode)
            {
                *OutMatchedBoneNode = CurrentNode;
            }

            return Skeleton;
        }
    }

    return nullptr;
}

bool FFBXImporter::ApplyRigidParentWeightFallback(FbxNode* MeshNode, USkeleton* Skeleton, int32 BoneIndex, FbxNode* BoneNode, FSkeletalSubMesh* Mesh)
{
    if (!MeshNode || !Skeleton || !BoneNode || !Mesh || BoneIndex < 0)
    {
        return false;
    }

    const TArray<FBoneInfo>& Bones = Skeleton->GetBones();
    if (BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return false;
    }

    Mesh->InverseBindPoseMatrices.clear();
    Mesh->InverseBindPoseMatrices.resize(Bones.size(), FMatrix::Identity);
    Mesh->BoneBindGlobalMatrices.clear();
    Mesh->BoneBindGlobalMatrices.resize(Bones.size(), FMatrix::Identity);

    FMatrix MeshBindGlobal = ConvertFbxMatrix(MeshNode->EvaluateGlobalTransform());
    FMatrix BoneBindGlobal = ConvertFbxMatrix(BoneNode->EvaluateGlobalTransform());

    Mesh->MeshBindInverseScale = GetSkeletonMeshBindInverseScale(Skeleton, MeshBindGlobal);
    Mesh->InverseBindPoseMatrices[BoneIndex] = MeshBindGlobal * BoneBindGlobal.GetInverse();
    Mesh->BoneBindGlobalMatrices[BoneIndex] = BoneBindGlobal;

    for (size_t VertexIndex = 0; VertexIndex < BoneWeighting.size(); ++VertexIndex)
    {
        BoneWeighting[VertexIndex].clear();
        BoneWeighting[VertexIndex].push_back({ static_cast<uint16>(BoneIndex), 1.0f });
    }

    return true;
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
