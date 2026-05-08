#include "SkeletalMeshSceneProxy.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/SkinnedMeshComponent.h"
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Renderer.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent) : FMeshSceneProxy(InComponent)
{
	UpdateShadow();
}

void FSkeletalMeshSceneProxy::UpdateShadow() 
{
    UMeshComponent* Mesh = GetMeshComponent();
    bCastShadow          = Mesh ? Mesh->ShouldCastShadow() : true;
}

UMeshComponent* FSkeletalMeshSceneProxy::GetMeshComponent() const
{
    return static_cast<USkeletalMeshComponent*>(Owner);
}

// TODO: Finish this stub once the class type rolls in
void FSkeletalMeshSceneProxy::RebuildSectionRenderData() 
{
    //USkinnedMeshComponent* SMC    = static_cast<USkinnedMeshComponent*>(GetMeshComponent());
    //USkeletalMesh*          Mesh  = SMC->GetSkeletalMesh();
    //if (!Mesh || !Mesh->GetSkeletalMeshAsset())
    //{
    //    LODData[0].MeshBuffer = nullptr;
    //    LODData[0].SectionRenderData.clear();
    //    LODData[0].OwnedMaterialCBs.clear();

    //    LODCount   = 1;
    //    CurrentLOD = 0;
    //    MeshBuffer = nullptr;
    //    SectionRenderData.clear();
    //    ActiveOwnedMaterialCBs.clear();
    //    return;
    //}

    //ID3D11Device*        Device  = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
    //ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;

    //const auto& Slots     = Mesh->GetStaticMaterials();
    //const auto& Overrides = SMC->GetOverrideMaterials();
    //LODCount              = Mesh->GetLODCount();

    //const auto& Sections    = Mesh->GetLODSections(lod);
    //LODData[0].MeshBuffer = Mesh->GetLODMeshBuffer(lod);
    //LODData[0].SectionRenderData.clear();
    //LODData[0].OwnedMaterialCBs.clear();
    //LODData[0].SectionRenderData.reserve(Sections.size());
    //LODData[0].OwnedMaterialCBs.reserve(Sections.size());

    //for (const FStaticMeshSection& Section : Sections)
    //{
    //    FMeshSectionRenderData Draw;
    //    Draw.FirstIndex    = Section.FirstIndex;
    //    Draw.IndexCount    = Section.NumTriangles * 3;
    //    Draw.Blend         = EBlendState::Opaque;
    //    Draw.DepthStencil  = EDepthStencilState::Default;
    //    Draw.Rasterizer    = ERasterizerState::SolidBackCull;
    //    Draw.MaterialCB[0] = nullptr;
    //    Draw.MaterialCB[1] = nullptr;

    //    UMaterial*  Mat           = nullptr;
    //    const int32 MaterialIndex = Section.MaterialIndex;
    //    if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Slots.size()))
    //    {
    //        if (MaterialIndex < static_cast<int32>(Overrides.size()) && Overrides[MaterialIndex])
    //        {
    //            Mat = Overrides[MaterialIndex];
    //        }
    //        else if (Slots[MaterialIndex].MaterialInterface)
    //        {
    //            Mat = Slots[MaterialIndex].MaterialInterface;
    //        }
    //    }

    //    if (Mat)
    //    {
    //        TryGetTextureSRV(Mat, { MaterialSemantics::DiffuseTextureSlot, "BaseColorTexture", "AlbedoTexture", "BaseTexture", "DiffuseMap" }, Draw.DiffuseSRV);
    //        TryGetTextureSRV(Mat, { MaterialSemantics::NormalTextureSlot, "NormalMap", "NormalMapTexture", "BumpTexture", "BumpMap" }, Draw.NormalSRV);
    //        TryGetTextureSRV(Mat, { MaterialSemantics::SpecularTextureSlot, "SpecularMap", "SpecularMapTexture", "SpecularMask", "SpecularMaskTexture", "GlossMap" }, Draw.SpecularSRV);
    //    }

    //    auto MaterialCB = BuildMeshMaterialCB(Mat, Device, Context, Draw.DiffuseSRV, Draw.NormalSRV, Draw.SpecularSRV);
    //    if (MaterialCB)
    //    {
    //        Draw.MaterialCB[0] = MaterialCB->GetConstantBuffer();
    //        LODData[0].OwnedMaterialCBs.push_back(std::move(MaterialCB));
    //    }

    //    LODData[0].SectionRenderData.push_back(Draw);
    //    SortSectionRenderDataByMaterial(LODData[0].SectionRenderData);
    }