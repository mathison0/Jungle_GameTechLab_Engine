#include "PrimitiveDrawCommandBuilder.h"

#include "Component/BillboardComponent.h"
#include "Component/FireballComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/ProceduralMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Particle/ParticleRendererProperties.h"
#include "Particle/ParticleSystemComponent.h"


#include "Core/Logging/SkinningStats.h"
#include "Core/ResourceManager.h"
#include "Engine/Asset/StaticMesh.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/RenderBus.h"

#include <algorithm>
#include <cmath>
#include <variant>
#include <unordered_map>
#include <unordered_set>

namespace
{
    void BuildBoneMatrixConstants(const USkeletalMeshComponent* SkeletalMeshComp, FBoneMatrixConstants& OutConstants)
    {
        OutConstants = {};
        if (!SkeletalMeshComp)
        {
            return;
        }

        const TArray<FMatrix>& SkinningMatrices = SkeletalMeshComp->GetSkinningMatrices();
        const uint32 BoneCount = static_cast<uint32>((std::min)(
            SkinningMatrices.size(),
            static_cast<size_t>(MaxGPUSkinBones)));

        OutConstants.BoneCount = BoneCount;
        for (uint32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            OutConstants.BoneMatrices[BoneIndex] = SkinningMatrices[BoneIndex];
        }
    }

    FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
    {
        const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
        const UBillboardComponent* Billboard = static_cast<const UBillboardComponent*>(Primitive);
        return UBillboardComponent::MakeBillboardWorldMatrix(
            WorldMatrix.GetOrigin(),
            Billboard->GetBillboardWorldScale(),
            RenderBus.GetCameraForward(),
            RenderBus.GetCameraRight(),
            RenderBus.GetCameraUp());
    }

    int32 SelectLODLevel(const FVector& CameraPos, const FAABB& Bounds, const FMatrix& ProjMatrix, int32 ValidLODCount)
    {
        bool IsOrthoGraphic = (std::abs(ProjMatrix.M[3][3] - 1.0f) < 1e-4f);
        if (ValidLODCount <= 1 || IsOrthoGraphic) return 0;

        const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
        const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
        const float SphereRadius = std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);

        const FVector Diff = Center - CameraPos;
        const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

        if (Dist <= 1e-4f) return 0;

        const float ProjectedRadius = (SphereRadius / Dist) * ProjMatrix.M[2][1];
        const float ScreenCoverage = ProjectedRadius;

        static constexpr float Thresholds[] = { 0.15f, 0.08f, 0.05f, 0.02f };
        static constexpr int32 ThresholdCount = static_cast<int32>(sizeof(Thresholds) / sizeof(Thresholds[0]));

        const int32 MaxLOD = ValidLODCount - 1;
        for (int32 LOD = 0; LOD < MaxLOD; ++LOD)
        {
            float Threshold = (LOD < ThresholdCount) ? Thresholds[LOD] : 0.0f;
            if (ScreenCoverage >= Threshold)
                return LOD;
        }

        return MaxLOD;
    }

    UMaterialInterface* ResolveDrawMaterial(UMaterialInterface* Material)
    {
        return Material ? Material : FResourceManager::Get().GetMaterial("DefaultWhite");
    }

    UTexture* ResolveParticleTexture(const UParticleModuleRequired* RequiredModule)
    {
        if (!RequiredModule)
        {
            return nullptr;
        }

        if (UMaterialInterface* Material = RequiredModule->GetMaterial())
        {
            FMaterialParamValue DiffuseMap;
            if (Material->GetParam("DiffuseMap", DiffuseMap) &&
                DiffuseMap.Type == EMaterialParamType::Texture &&
                std::holds_alternative<UTexture*>(DiffuseMap.Value))
            {
                if (UTexture* Texture = std::get<UTexture*>(DiffuseMap.Value))
                {
                    return Texture;
                }
            }
        }

        if (const FTextureAtlasResource* SubUV = FResourceManager::Get().FindSubUVExact(RequiredModule->GetSubUVName()))
        {
            return SubUV->Texture;
        }

        return nullptr;
    }

    double CalculateAverageBoneInfluence(const TArray<FSkeletalMeshVertex>& Vertices)
    {
        if (Vertices.empty())
        {
            return 0.0;
        }

        uint64 InfluenceCount = 0;
        for (const FSkeletalMeshVertex& Vertex : Vertices)
        {
            for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
            {
                if (Vertex.BoneWeights[InfluenceIndex] > 0.0f)
                {
                    ++InfluenceCount;
                }
            }
        }

        return static_cast<double>(InfluenceCount) / static_cast<double>(Vertices.size());
    }

    uint64 CalculateUniqueSectionVertexCount(
        const TArray<uint32>& Indices,
        uint32 StartIndex,
        uint32 IndexCount)
    {
        if (IndexCount == 0 || StartIndex >= Indices.size())
        {
            return 0;
        }

        const uint64 EndIndex = (std::min<uint64>)(
            static_cast<uint64>(StartIndex) + static_cast<uint64>(IndexCount),
            static_cast<uint64>(Indices.size()));

        std::unordered_set<uint32> UniqueVertexIndices;
        UniqueVertexIndices.reserve(static_cast<size_t>(EndIndex - StartIndex));

        for (uint64 IndexOffset = StartIndex; IndexOffset < EndIndex; ++IndexOffset)
        {
            UniqueVertexIndices.insert(Indices[static_cast<size_t>(IndexOffset)]);
        }

        return static_cast<uint64>(UniqueVertexIndices.size());
    }

    struct FSkeletalMeshSkinningStatCache
    {
        uint64 VertexCount = 0;
        uint64 IndexCount = 0;
        uint64 SectionCount = 0;
        uint64 BoneCount = 0;
        double AvgBoneInfluence = 0.0;
        TArray<uint64> SectionVertexCounts;
    };

    const FSkeletalMeshSkinningStatCache& GetSkeletalMeshSkinningStatCache(const USkeletalMesh* Mesh)
    {
        static std::unordered_map<const FSkeletalMesh*, FSkeletalMeshSkinningStatCache> Cache;

        const FSkeletalMesh* MeshData = Mesh->GetMeshData();
        const TArray<FSkeletalMeshVertex>& Vertices = Mesh->GetVertices();
        const TArray<uint32>& Indices = Mesh->GetIndices();
        const TArray<FStaticMeshSection>& Sections = Mesh->GetSections();
        const TArray<FBoneInfo>& Bones = Mesh->GetBones();

        const uint64 VertexCount = static_cast<uint64>(Vertices.size());
        const uint64 IndexCount = static_cast<uint64>(Indices.size());
        const uint64 SectionCount = static_cast<uint64>(Sections.size());
        const uint64 BoneCount = static_cast<uint64>(Bones.size());

        auto It = Cache.find(MeshData);
        if (It != Cache.end()
            && It->second.VertexCount == VertexCount
            && It->second.IndexCount == IndexCount
            && It->second.SectionCount == SectionCount
            && It->second.BoneCount == BoneCount)
        {
            return It->second;
        }

        FSkeletalMeshSkinningStatCache Entry;
        Entry.VertexCount = VertexCount;
        Entry.IndexCount = IndexCount;
        Entry.SectionCount = SectionCount;
        Entry.BoneCount = BoneCount;
        Entry.AvgBoneInfluence = CalculateAverageBoneInfluence(Vertices);
        Entry.SectionVertexCounts.resize(Sections.size());

        for (uint64 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            const FStaticMeshSection& Section = Sections[static_cast<size_t>(SectionIndex)];
            Entry.SectionVertexCounts[static_cast<size_t>(SectionIndex)] =
                CalculateUniqueSectionVertexCount(Indices, Section.StartIndex, Section.IndexCount);
        }

        auto Result = Cache.insert_or_assign(MeshData, Entry);
        return Result.first->second;
    }
}

bool FPrimitiveDrawCommandBuilder::CollectPrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags,
                                                    EViewMode ViewMode, FRenderBus& RenderBus,
                                                    FMeshBufferManager& MeshBufferManager) const
{
    (void)ViewMode;

    if (Primitive == nullptr || !Primitive->IsVisible()) return true;

    switch (Primitive->GetPrimitiveType())
    {
    case EPrimitiveType::EPT_StaticMesh:
    {
        if (!ShowFlags.bPrimitives) return true;

        UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
        const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();

        if (!StaticMesh || !StaticMesh->HasValidMeshData()) return true;

        FVector CameraPos = RenderBus.GetCameraPosition();
        FMatrix ProjMatrix = RenderBus.GetProj();
        FAABB Bounds = StaticMeshComp->GetWorldAABB();
        const int32 ValidLODCount = StaticMesh->GetValidLODCount();

        int32 SelectedLOD = 0;
        if (ShowFlags.bEnableLOD)
        {
            SelectedLOD = SelectLODLevel(CameraPos, Bounds, ProjMatrix, ValidLODCount);
        }

        FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh, SelectedLOD);
        if (!MeshBuffer) return true;

        const FStaticMesh* MeshData = StaticMesh->GetMeshData(SelectedLOD);
        const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const FStaticMeshSection& Section = Sections[SectionIdx];
            UMaterialInterface* Material = ResolveDrawMaterial(Cast<UMaterialInterface>(StaticMeshComp->GetMaterial(SectionIdx)));

            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::StaticMesh;
            Cmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
            Cmd.MeshBuffer = MeshBuffer;

            Cmd.SectionIndexStart = Section.StartIndex;
            Cmd.SectionIndexCount = Section.IndexCount;
            Cmd.Material = Material;

            Cmd.WorldAABB = StaticMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }

        return true;
    }

    case EPrimitiveType::EPT_SkeletalMesh:
    {
        if (!ShowFlags.bPrimitives || !ShowFlags.bSkeletalMesh) return true;

        USkeletalMeshComponent* SkeletalMeshComp = static_cast<USkeletalMeshComponent*>(Primitive);
        USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMesh();

        if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData()) return true;

        SkeletalMeshComp->EnsureSkinningUpdated();
        const bool bNeedsUpload = SkeletalMeshComp->ConsumeRenderStateDirty();

        const ESkinningMode SkinningMode = SkeletalMeshComp->GetResolvedSkinningMode();
        const bool bUseGPUSkinning = SkinningMode == ESkinningMode::GPU;
        const FBoneWeightHeatmapViewState& BoneWeightHeatmapState = RenderBus.GetBoneWeightHeatmapViewState();
        const bool bUseBoneWeightHeatmap =
            ViewMode == EViewMode::BoneWeightHeatmap &&
            BoneWeightHeatmapState.bEnabled &&
            BoneWeightHeatmapState.SelectedBoneIndex >= 0 &&
            BoneWeightHeatmapState.SelectedBoneIndex < static_cast<int32>(SkeletalMesh->GetBones().size());
        uint32 BoneMatrixConstantsIndex = InvalidBoneMatrixConstantsIndex;
        FConstantBuffer* BoneMatrixConstantBuffer = nullptr;
        if (bUseGPUSkinning)
        {
            FBoneMatrixConstants BoneMatrixConstants = {};
            BuildBoneMatrixConstants(SkeletalMeshComp, BoneMatrixConstants);

            BoneMatrixConstantBuffer = MeshBufferManager.GetGPUSkeletalBoneMatrixBuffer(
                SkeletalMeshComp->GetUUID(),
                BoneMatrixConstants,
                bNeedsUpload);

            if (!BoneMatrixConstantBuffer)
            {
                BoneMatrixConstantsIndex = RenderBus.AllocateBoneMatrixConstants();
                if (FBoneMatrixConstants* Constants = RenderBus.GetMutableBoneMatrixConstants(BoneMatrixConstantsIndex))
                {
                    *Constants = BoneMatrixConstants;
                }
            }
        }
        const TArray<uint32>& Indices = SkeletalMesh->GetIndices(); // 이건 immutable이라 걍 asset에서 들고와도 댐
        const FSkeletalMeshSkinningStatCache& SkinningStatCache = GetSkeletalMeshSkinningStatCache(SkeletalMesh);
        FSkinningStats::Get().AddVisibleSkinnedMesh(
            SkinningStatCache.VertexCount,
            static_cast<uint32>(SkinningStatCache.BoneCount),
            SkinningStatCache.AvgBoneInfluence,
            bUseGPUSkinning);

        FMeshBuffer* MeshBuffer = bUseGPUSkinning
            ? MeshBufferManager.GetGPUSkeletalMeshBuffer(SkeletalMesh)
            : MeshBufferManager.GetCPUSkeletalMeshBuffer(
                SkeletalMeshComp->GetUUID(),
                SkeletalMesh,
                SkeletalMeshComp->GetSkinnedVertices(),
                Indices,
                SkeletalMeshComp->ConsumeCPUSkinnedVertexBufferDirty());
        if (!MeshBuffer) return true;

        const TArray<FStaticMeshSection>& Sections = SkeletalMesh->GetSections();
        if (Sections.empty()) // fallback
        {
            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::SkeletalMesh;
            Cmd.VertexFactoryType = EVertexFactoryType::SkeletalMesh;
            Cmd.MeshBuffer = MeshBuffer;
            Cmd.bUseBoneMatrixConstants = bUseGPUSkinning;
            Cmd.BoneMatrixConstantsIndex = BoneMatrixConstantsIndex;
            Cmd.BoneMatrixConstantBuffer = BoneMatrixConstantBuffer;
            Cmd.bUseBoneWeightHeatmap = bUseBoneWeightHeatmap;
            Cmd.BoneWeightHeatmapBoneIndex = bUseBoneWeightHeatmap
                ? BoneWeightHeatmapState.SelectedBoneIndex
                : -1;
            Cmd.AvgBoneInfluencePerVertex = static_cast<float>(SkinningStatCache.AvgBoneInfluence);
            Cmd.SkinningWorkVertexCount = SkinningStatCache.VertexCount;
            Cmd.SectionIndexStart = 0;
            Cmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
            Cmd.Material = ResolveDrawMaterial(Cast<UMaterialInterface>(SkeletalMeshComp->GetMaterial(0)));
            Cmd.WorldAABB = SkeletalMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
            return true;
        }

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const FStaticMeshSection& Section = Sections[SectionIdx];
            if (Section.IndexCount == 0)
            {
                continue;
            }

            UMaterialInterface* Material = ResolveDrawMaterial(Cast<UMaterialInterface>(SkeletalMeshComp->GetMaterial(SectionIdx)));

            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::SkeletalMesh;
            Cmd.VertexFactoryType = EVertexFactoryType::SkeletalMesh;
            Cmd.MeshBuffer = MeshBuffer;
            Cmd.bUseBoneMatrixConstants = bUseGPUSkinning;
            Cmd.BoneMatrixConstantsIndex = BoneMatrixConstantsIndex;
            Cmd.BoneMatrixConstantBuffer = BoneMatrixConstantBuffer;
            Cmd.bUseBoneWeightHeatmap = bUseBoneWeightHeatmap;
            Cmd.BoneWeightHeatmapBoneIndex = bUseBoneWeightHeatmap
                ? BoneWeightHeatmapState.SelectedBoneIndex
                : -1;
            Cmd.AvgBoneInfluencePerVertex = static_cast<float>(SkinningStatCache.AvgBoneInfluence);
            Cmd.SkinningWorkVertexCount =
                SectionIdx < static_cast<int32>(SkinningStatCache.SectionVertexCounts.size())
                    ? SkinningStatCache.SectionVertexCounts[SectionIdx]
                    : 0;

            Cmd.SectionIndexStart = Section.StartIndex;
            Cmd.SectionIndexCount = Section.IndexCount;
            Cmd.Material = Material;

            Cmd.WorldAABB = SkeletalMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }

        return true;
    }

    case EPrimitiveType::EPT_Text:
    {
        if (!ShowFlags.bBillboardText) return true;

        UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
        const FFontResource* Font = TextComp->GetFont();
        if (!Font || !Font->IsLoaded()) return true;

        const FString& Text = TextComp->GetText();
        if (Text.empty()) return true;

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Font;
        Cmd.VertexFactoryType = EVertexFactoryType::Text;
        Cmd.SourcePrimitive = Primitive;
        Cmd.PerObjectConstants = FPerObjectConstants{ TextComp->GetWorldMatrix(), TextComp->GetColor() };
        Cmd.Constants.Font.Text = &Text;
        Cmd.Constants.Font.Font = Font;
        Cmd.Constants.Font.Scale = TextComp->GetFontSize();

        RenderBus.AddCommand(ERenderPass::Font, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_SubUV:
    {
        USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
        const FTextureAtlasResource* Atlas = SubUVComp->GetSubUV();
        if (!Atlas || !Atlas->IsLoaded()) return true;

        FRenderCommand Cmd = {};
        Cmd.PerObjectConstants = FPerObjectConstants{
            MakeViewBillboardMatrix(Primitive, RenderBus),
            FColor::White().ToVector4() };
        Cmd.SourcePrimitive = Primitive;
        Cmd.Type = ERenderCommandType::SubUV;
        Cmd.VertexFactoryType = EVertexFactoryType::SubUV;
        Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_SubUV);
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
        Cmd.Constants.SubUV.Atlas = Atlas;
        Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
        Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
        Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();

        RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_Billboard:
    {
        UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(Primitive);
        UTexture* Texture = BillboardComp->GetTexture();

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Billboard;
        Cmd.VertexFactoryType = EVertexFactoryType::Billboard;
        Cmd.SourcePrimitive = Primitive;
        Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Billboard);
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
        Cmd.PerObjectConstants = FPerObjectConstants{
            MakeViewBillboardMatrix(Primitive, RenderBus),
            FColor::White().ToVector4() };
        Cmd.Constants.Billboard.Texture = Texture;
        Cmd.Constants.Billboard.Width = BillboardComp->GetWidth();
        Cmd.Constants.Billboard.Height = BillboardComp->GetHeight();
        Cmd.Constants.Billboard.Color = BillboardComp->GetColor();

        RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_FOG:
    {
        if (!ShowFlags.bFog)
            return true;
        UHeightFogComponent* HeightFogComp = static_cast<UHeightFogComponent*>(Primitive);

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Primitive;
        Cmd.VertexFactoryType = EVertexFactoryType::Primitive;
        Cmd.Constants.Fog.FogDensity = HeightFogComp->GetFogDensity();
        Cmd.Constants.Fog.FogColor = HeightFogComp->GetFogInscatteringColor();
        Cmd.Constants.Fog.HeightFalloff = HeightFogComp->GetHeightFalloff();
        Cmd.Constants.Fog.FogHeight = HeightFogComp->GetFogHeight();
        Cmd.Constants.Fog.FogStartDistance = HeightFogComp->GetFogStartDistance();
        Cmd.Constants.Fog.FogMaxOpacity = HeightFogComp->GetFogMaxOpacity();
        Cmd.Constants.Fog.FogCutoffDistance = HeightFogComp->GetFogCutoffDistance();

        RenderBus.AddCommand(ERenderPass::Fog, Cmd);
        return true;
    }

    case EPrimitiveType::EPT_Fireball:
    {
        UFireballComponent* FireballComp = static_cast<UFireballComponent*>(Primitive);

        FLightData LightData = {};
        LightData.Intensity = FireballComp->GetIntensity();
        LightData.Radius = FireballComp->GetRadius();
        LightData.RadiusFalloff = FireballComp->GetRadiusFallOff();
        LightData.WorldPos = FireballComp->GetWorldLocation();

        FColor Color = FireballComp->GetLinearColor();
        LightData.Color.X = Color.R;
        LightData.Color.Y = Color.G;
        LightData.Color.Z = Color.B;
        return true;
    }

    case EPrimitiveType::EPT_ProceduralMesh:
    {
        if (!ShowFlags.bPrimitives)
            return true;

        UProceduralMeshComponent* ProcMeshComp = static_cast<UProceduralMeshComponent*>(Primitive);
        const TArray<UProceduralMeshComponent::FMeshSection>& Sections = ProcMeshComp->GetSections();

        if (!ProcMeshComp || Sections.empty())
            return true;

        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const UProceduralMeshComponent::FMeshSection& Section = Sections[SectionIdx];
            FMeshBuffer* MeshBuffer = nullptr;
            MeshBuffer = MeshBufferManager.GetProcMeshBuffer(ProcMeshComp->GetUUID(), Section.Vertices, Section.Indices);

            if (!MeshBuffer)
                break;

            UMaterialInterface* Material = Cast<UMaterialInterface>(ProcMeshComp->GetMaterial(SectionIdx));

            FRenderCommand Cmd = {};
            Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
            Cmd.SourcePrimitive = Primitive;
            Cmd.Type = ERenderCommandType::StaticMesh;
            Cmd.VertexFactoryType = EVertexFactoryType::ProceduralMesh;
            Cmd.MeshBuffer = MeshBuffer;

            Cmd.SectionIndexStart = 0;
            Cmd.SectionIndexCount = static_cast<uint32>(Section.Indices.size());
            Cmd.Material = Material;

            Cmd.WorldAABB = ProcMeshComp->GetWorldAABB();

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }
        return true;
    }

	case EPrimitiveType::EPT_ParticleSystem:
    {
        if (!ShowFlags.bPrimitives)
            return true;

        UParticleSystemComponent* ParticleSystemComponent = Cast<UParticleSystemComponent>(Primitive);
        if (ParticleSystemComponent == nullptr)
        {
            UE_LOG("ParticleSystem_Cast_reference_Wrong");
            return false;
        }

        // Cycle 10c 계층 분리: Component는 instance 순회 hook만 (RenderCommand 모름).
        // Instance는 자기 buffer만 갱신, RenderCommand 매핑은 Builder가 책임.
        ParticleSystemComponent->BuildInstanceData();

        // Cycle 10c: Builder가 instance와 RenderCommand 사이의 매핑 책임.
        // - Instance::Get*Data() getter로 type별 데이터를 const 포인터+count로 회수 (제로 복사)
        // - RenderMode switch로 Cmd의 type별 슬롯 + VertexFactoryType 직접 채움
        // - Instance는 RenderCommand를 모름 (단방향 dependency)
        const TArray<FParticleEmitterInstance*>& EmitterInstances = ParticleSystemComponent->GetEmitterInstances();
        for (int32 EmitterIdx = 0; EmitterIdx < static_cast<int32>(EmitterInstances.size()); ++EmitterIdx)
        {
            FParticleEmitterInstance* Instance = EmitterInstances[EmitterIdx];
            if (!Instance || Instance->GetActiveParticleCount() == 0)
            {
                continue;
            }

            // RenderMode 결정 — RendererProperties single source.
            const FCompiledParticleLODData* CompiledLOD = Instance->GetCurrentCompiledLODData();
            UParticleRendererProperties* RendererProperties =
                CompiledLOD ? CompiledLOD->RendererProperties : nullptr;
            EParticleEmitterRenderMode RenderMode =
                CompiledLOD ? CompiledLOD->RenderMode : EParticleEmitterRenderMode::Sprite;
            if (!RendererProperties)
            {
                UParticleLODLevel* LOD = Instance->GetCurrentLODLevel();
                if (!LOD)
                {
                    continue;
                }

                RendererProperties = LOD->GetEffectiveRendererProperties();
                RenderMode = LOD->GetEffectiveRenderMode();
            }

            // Cmd 기본 필드 (generic, type 무관) 먼저 채움
            FRenderCommand Cmd = {};
            Cmd.SourcePrimitive = Primitive;
            Cmd.PerObjectConstants = FPerObjectConstants(FMatrix::Identity, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
            Cmd.Type = ERenderCommandType::Primitive;
            Cmd.WorldAABB = ParticleSystemComponent->GetWorldAABB();

            // RenderMode별로 적절한 getter 호출 + 해당 type 슬롯만 채움.
            // 다른 type 슬롯은 zero-init된 nullptr/0 유지 (silent bug 위험 3 회피).
            // Mesh/Ribbon/Beam은 base default가 nullptr 반환 → Cycle 11+에서 derived가 override해야 데이터 채워짐.
            uint32 Count = 0;
            bool bHasData = false;
            switch (RenderMode)
            {
            case EParticleEmitterRenderMode::Sprite:
                Cmd.ParticleInstances = Instance->GetSpriteInstanceData(Count);
                Cmd.ParticleInstanceCount = Count;
                Cmd.VertexFactoryType = EVertexFactoryType::SpriteParticle;
                bHasData = (Cmd.ParticleInstances != nullptr && Count > 0);
                break;
            case EParticleEmitterRenderMode::Mesh:
                Cmd.MeshParticleInstances = Instance->GetMeshInstanceData(Count);
                Cmd.MeshParticleInstanceCount = Count;
                Cmd.VertexFactoryType = EVertexFactoryType::MeshParticle;
                // Mesh renderer properties의 Mesh asset 조회 + MeshBuffer 세팅 + Material 세팅.
                // PerObject CB는 Identity Model (instance VB가 World 합성 담당 — Sprite와 동일 원칙).
                if (const UParticleMeshRendererProperties* MeshRenderer = Cast<UParticleMeshRendererProperties>(RendererProperties))
                {
                    if (UStaticMesh* MeshAsset = MeshRenderer->GetMesh())
                    {
                        Cmd.MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(MeshAsset, 0);
                        Cmd.SectionIndexStart = 0;
                        Cmd.SectionIndexCount = Cmd.MeshBuffer ? Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount() : 0;
                        Cmd.Material = MeshRenderer->GetEffectiveMaterial();
                    }
                }
                bHasData = (Cmd.MeshParticleInstances != nullptr && Count > 0 && Cmd.MeshBuffer != nullptr);
                break;
            case EParticleEmitterRenderMode::Ribbon:
                Cmd.RibbonVertices = Instance->GetRibbonVertexData(Count);
                Cmd.RibbonVertexCount = Count;
                Cmd.VertexFactoryType = EVertexFactoryType::RibbonParticle;
                if (const UParticleRibbonRendererProperties* RibbonRenderer = Cast<UParticleRibbonRendererProperties>(RendererProperties))
                {
                    Cmd.Material = RibbonRenderer->GetMaterial();
                }
                bHasData = (Cmd.RibbonVertices != nullptr && Count > 0);
                break;
            case EParticleEmitterRenderMode::Beam:
                Cmd.BeamVertices = Instance->GetBeamVertexData(Count);
                Cmd.BeamVertexCount = Count;
                Cmd.VertexFactoryType = EVertexFactoryType::BeamParticle;
                bHasData = (Cmd.BeamVertices != nullptr && Count > 0);
                break;
            default:
                break;
            }

            // active particle 0개거나 base nullptr fallback(Mesh/Ribbon/Beam 본 cycle 미구현) — Cmd 발행 skip.
            // 빈 슬롯으로 그리려 하면 D3D 에러 — silent bug 위험 2 회피.
            if (!bHasData)
            {
                continue;
            }

            // Sprite는 RequiredModule + SubUV/Atlas로 Material/Texture/SubUVGrid 결정.
            // Mesh는 switch case에서 MeshTD->GetEffectiveMaterial()로 이미 Material 세팅됨 → Sprite 분기에 안 들어감.
            // Mesh의 ParticleTexture는 Material의 DiffuseMap에서 추출 (Sprite의 ResolveParticleTexture 패턴 일부 재사용).
            if (RenderMode == EParticleEmitterRenderMode::Sprite)
            {
                const UParticleLODLevel* LODLevel = EmitterInstances[EmitterIdx]
                    ? EmitterInstances[EmitterIdx]->GetCurrentLODLevel()
                    : nullptr;
                const UParticleModuleRequired* RequiredModule = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
                const USubUVModule* SubUV = nullptr;
                if (LODLevel)
                {
                    for (UParticleModule* Module : LODLevel->GetModules())
                    {
                        if (USubUVModule* Found = Cast<USubUVModule>(Module))
                        {
                            SubUV = Found;
                            break;
                        }
                    }
                }
                const FTextureAtlasResource* Atlas = SubUV ? SubUV->GetCachedSubUV() : nullptr;
                Cmd.Material = RequiredModule ? RequiredModule->GetMaterial() : nullptr;
                Cmd.ParticleTexture = (Atlas && Atlas->IsLoaded()) ? Atlas->Texture : ResolveParticleTexture(RequiredModule);
                Cmd.ParticleSubUVColumns = Atlas ? Atlas->Columns : (RequiredModule ? static_cast<uint32>(RequiredModule->GetSubImagesHorizontal()) : 1);
                Cmd.ParticleSubUVRows = Atlas ? Atlas->Rows : (RequiredModule ? static_cast<uint32>(RequiredModule->GetSubImagesVertical()) : 1);
            }
            else if (RenderMode == EParticleEmitterRenderMode::Mesh || RenderMode == EParticleEmitterRenderMode::Ribbon)
            {
                // Material의 DiffuseMap에서 ParticleTexture 추출. 없으면 RenderPass가 default white SRV로 fallback.
                if (Cmd.Material)
                {
                    FMaterialParamValue DiffuseMap;
                    if (Cmd.Material->GetParam("DiffuseMap", DiffuseMap) &&
                        DiffuseMap.Type == EMaterialParamType::Texture &&
                        std::holds_alternative<UTexture*>(DiffuseMap.Value))
                    {
                        Cmd.ParticleTexture = std::get<UTexture*>(DiffuseMap.Value);
                    }
                }
            }

            RenderBus.AddCommand(ERenderPass::Particle, Cmd);
        }
        return true;
    }

    default:
        return false;
    }
}
