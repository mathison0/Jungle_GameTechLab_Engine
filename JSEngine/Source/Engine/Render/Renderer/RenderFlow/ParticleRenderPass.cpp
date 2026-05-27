#include "ParticleRenderPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/Resource/Texture.h"
#include "Render/SubUVBatcher.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Scene/RenderCommand.h"

#include <algorithm>

// Cycle 10a baseline: Mesh/Ribbon/Beam 슬롯 6개 추가 후 FRenderCommand sizeof.
// 추가 멤버 추정: 3 포인터(8×3=24) + 3 uint32(4×3=12) + 정렬 padding ≈ +48 bytes.
// 이전 cycle 빌드에서 측정 안 했으므로 이 값이 첫 baseline. 추후 슬롯 추가 시 본 assert로 회귀 감지.
static_assert(sizeof(FRenderCommand) == 464, "Cycle 10a baseline: FRenderCommand expected 464 bytes on x64 MSVC after particle slot extension");

namespace
{
    struct FSpriteParticleCB
    {
        uint32 SubUVColumns;
        uint32 SubUVRows;
        float  Padding[2];
    };

    FShaderProgram* GetSpriteParticleProgram()
    {
        const FVertexFactoryDesc& Desc = FVertexFactoryRegistry::Get(EVertexFactoryType::SpriteParticle);

        FShaderStageKey VSKey;
        VSKey.FilePath = Desc.VertexShaderPath;
        VSKey.EntryPoint = Desc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::ParticleSprite;
        PSKey.EntryPoint = "SpriteParticlePS";
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey, PSKey, nullptr, nullptr, &Desc.VertexLayout);
    }

    // Cycle 11: Mesh particle 전용 shader program.
    // SpriteParticle 패턴과 동일 — Registry의 Desc로 VS/Layout 받고 PS는 직접 지정.
    FShaderProgram* GetMeshParticleProgram()
    {
        const FVertexFactoryDesc& Desc = FVertexFactoryRegistry::Get(EVertexFactoryType::MeshParticle);

        FShaderStageKey VSKey;
        VSKey.FilePath = Desc.VertexShaderPath;
        VSKey.EntryPoint = Desc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::ParticleMesh;
        PSKey.EntryPoint = "MeshParticlePS";
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey, PSKey, nullptr, nullptr, &Desc.VertexLayout);
    }

    // Cycle 12: Ribbon particle 전용 shader program. slot 0 per-vertex only, no instancing.
    FShaderProgram* GetRibbonParticleProgram()
    {
        const FVertexFactoryDesc& Desc = FVertexFactoryRegistry::Get(EVertexFactoryType::RibbonParticle);

        FShaderStageKey VSKey;
        VSKey.FilePath = Desc.VertexShaderPath;
        VSKey.EntryPoint = Desc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::ParticleRibbon;
        PSKey.EntryPoint = "RibbonParticlePS";
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey, PSKey, nullptr, nullptr, &Desc.VertexLayout);
    }

    // Cycle 13a: Beam particle 전용 shader program. Ribbon 와 동일 카테고리 — slot 0 per-vertex only, no instancing.
    FShaderProgram* GetBeamParticleProgram()
    {
        const FVertexFactoryDesc& Desc = FVertexFactoryRegistry::Get(EVertexFactoryType::BeamParticle);

        FShaderStageKey VSKey;
        VSKey.FilePath = Desc.VertexShaderPath;
        VSKey.EntryPoint = Desc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::ParticleBeam;
        PSKey.EntryPoint = "BeamParticlePS";
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey, PSKey, nullptr, nullptr, &Desc.VertexLayout);
    }

    const UMaterial* ResolveBaseMaterial(const UMaterialInterface* MaterialInterface)
    {
        UMaterialInterface* MutableMaterial = const_cast<UMaterialInterface*>(MaterialInterface);
        if (const UMaterial* Material = Cast<UMaterial>(MutableMaterial))
        {
            return Material;
        }
        if (const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MutableMaterial))
        {
            return MaterialInstance->Parent;
        }
        return nullptr;
    }
}

bool FParticleRenderPass::Initialize()
{
    return true;
}

bool FParticleRenderPass::Release()
{
    QuadVertexBuffer.Release();
    QuadIndexBuffer.Release();
    InstanceBuffer.Release();
    SpriteParticleCB.Release();
    MeshInstanceBuffer.Release();
    RibbonVertexBuffer.Release();
    BeamVertexBuffer.Release();
    bGPUResourcesReady = false;
    return true;
}

bool FParticleRenderPass::EnsureGPUResources(ID3D11Device* Device)
{
    if (bGPUResourcesReady || !Device)
    {
        return bGPUResourcesReady;
    }

    // Quad: 4 vertices, 6 indices (2 triangles). XY in [-0.5, 0.5], UV [0,1] standard.
    const FSpriteParticleVertex Vertices[4] =
    {
        { FVector(-0.5f, -0.5f, 0.0f), FVector2(0.0f, 1.0f) },
        { FVector( 0.5f, -0.5f, 0.0f), FVector2(1.0f, 1.0f) },
        { FVector(-0.5f,  0.5f, 0.0f), FVector2(0.0f, 0.0f) },
        { FVector( 0.5f,  0.5f, 0.0f), FVector2(1.0f, 0.0f) },
    };
    QuadVertexBuffer.CreateRaw(Device, Vertices, 4, sizeof(FSpriteParticleVertex), false);

    TArray<uint32> Indices = { 0, 2, 1, 1, 2, 3 };
    QuadIndexBuffer.Create(Device, Indices, static_cast<uint32>(sizeof(uint32) * Indices.size()));

    InstanceBuffer.Create(Device, sizeof(FSpriteParticleInstanceData), 256);
    SpriteParticleCB.Create(Device, sizeof(FSpriteParticleCB));

    // Cycle 11: Mesh emitter용 per-instance VB. Sprite와 동일 grow-by-2x 패턴.
    MeshInstanceBuffer.Create(Device, sizeof(FMeshParticleInstanceData), 256);

    // Cycle 12: Ribbon emitter용 slot 0 dynamic VB. instancing 없음 — sizeof(FRibbonParticleVertex) stride.
    RibbonVertexBuffer.Create(Device, sizeof(FRibbonParticleVertex), 256);

    // Cycle 13a: Beam emitter용 slot 0 dynamic VB. Ribbon 와 동일 패턴 — sizeof(FBeamParticleVertex) stride.
    BeamVertexBuffer.Create(Device, sizeof(FBeamParticleVertex), 256);

    bGPUResourcesReady = QuadVertexBuffer.GetBuffer() != nullptr
        && QuadIndexBuffer.GetBuffer() != nullptr
        && InstanceBuffer.IsValid()
        && SpriteParticleCB.GetBuffer() != nullptr
        && MeshInstanceBuffer.IsValid()
        && RibbonVertexBuffer.IsValid()
        && BeamVertexBuffer.IsValid();
    return bGPUResourcesReady;
}

bool FParticleRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FParticleRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (!Context || !Context->RenderBus || !Context->DeviceContext || !Context->Device)
    {
        return true;
    }

    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Particle);
    if (Commands.empty())
    {
        return true;
    }

    if (!EnsureGPUResources(Context->Device))
    {
        return false;
    }

    // Cycle 10a: type-agnostic dispatch. Cmd.VertexFactoryType으로 4-way switch → 각 helper.
    // Cycle 11: Mesh helper도 본문 보유. Ribbon/Beam은 Cycle 12b/13b에서 본문 채움.
    // 단일 Pass + procedural switch 구조 (사용자 결정 3).
    bool bAnySpriteRendered = false;
    bool bAnyMeshRendered = false;
    for (const FRenderCommand& Cmd : Commands)
    {
        switch (Cmd.VertexFactoryType)
        {
        case EVertexFactoryType::SpriteParticle:
            RenderSpriteEmitter(Cmd, *Context);
            bAnySpriteRendered = true;
            break;
        case EVertexFactoryType::MeshParticle:
            RenderMeshEmitter(Cmd, *Context);
            bAnyMeshRendered = true;
            break;
        case EVertexFactoryType::RibbonParticle:
            RenderRibbonEmitter(Cmd, *Context);
            break;
        case EVertexFactoryType::BeamParticle:
            RenderBeamEmitter(Cmd, *Context);
            break;
        default:
            // 알 수 없는 VertexFactoryType는 Particle 패스의 dispatch 대상이 아님 — skip.
            break;
        }
    }

    //TODO
    //Slot 해제는 End logic에서 담당하도록 수정하는게 가독성에 유리함. 추후 진행
    // Slot 1을 다른 패스가 자동으로 미사용한다고 가정해도 위험. instance VB는 binding 해제.
    // Sprite/Mesh Cmd가 한 번이라도 처리됐을 때만 unbind 필요 (Ribbon/Beam은 slot 1 사용 안 함).
    if (bAnySpriteRendered || bAnyMeshRendered)
    {
        ID3D11Buffer* NullBuffer = nullptr;
        UINT NullStride = 0;
        UINT NullOffset = 0;
        Context->DeviceContext->IASetVertexBuffers(1, 1, &NullBuffer, &NullStride, &NullOffset);
    }

    return true;
}

// Function : Render single Sprite emitter command — extracted from previous DrawCommand body
// input : Cmd, Context
// Cmd : render command produced by PrimitiveDrawCommandBuilder/Instance::BuildInstanceData
// Context : render pass context (Device, DeviceContext, RenderResources, ...)
// output : One DrawIndexedInstanced call issued when InstanceBuffer is valid
void FParticleRenderPass::RenderSpriteEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    if (Cmd.ParticleInstances == nullptr || Cmd.ParticleInstanceCount == 0)
    {
        return;
    }

    if (Context.SubUVBatcher && Context.RenderBus)
    {
        Context.SubUVBatcher->Clear();

        UTexture* Texture = Cmd.ParticleTexture ? Cmd.ParticleTexture : FResourceManager::Get().GetTexture("DefaultWhite");
        const uint32 Columns = (Cmd.ParticleSubUVColumns > 0) ? Cmd.ParticleSubUVColumns : 1;
        const uint32 Rows = (Cmd.ParticleSubUVRows > 0) ? Cmd.ParticleSubUVRows : 1;
        const uint32 FrameCount = std::max(Columns * Rows, 1u);
        const FVector UnitScale(1.0f, 1.0f, 1.0f);

        for (uint32 Index = 0; Index < Cmd.ParticleInstanceCount; ++Index)
        {
            const FSpriteParticleInstanceData& Particle = Cmd.ParticleInstances[Index];
            Context.SubUVBatcher->AddSprite(
                Texture,
                Particle.Position,
                Context.RenderBus->GetCameraRight(),
                Context.RenderBus->GetCameraUp(),
                UnitScale,
                Particle.SubUVIndex % FrameCount,
                Columns,
                Rows,
                Particle.Size.X * 2.0f,
                Particle.Size.Y * 2.0f,
                Particle.Color,
                Particle.Rotation);
        }

        const bool bWireframe = Context.RenderBus->GetViewMode() == EViewMode::Wireframe;
        Context.SubUVBatcher->Flush(Context.DeviceContext, bWireframe);
        return;
    }

    FShaderProgram* Program = GetSpriteParticleProgram();
    if (!Program || !Program->VS || !Program->PS)
    {
        return;
    }

    ID3D11DeviceContext* DeviceContext = Context.DeviceContext;

    // Cycle 10a 비효율 인지: helper가 self-contained라 setup이 Cmd마다 반복.
    // d3d state cache가 diff 처리하므로 정확성에는 영향 없음. 추후 grouping/state-mgmt cycle에서 최적화 가능.
    Program->Bind(DeviceContext);

    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
    DeviceContext->OMSetDepthStencilState(DepthState, 0);
    ID3D11RasterizerState* RasterState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    DeviceContext->RSSetState(RasterState);
    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);

    ID3D11Buffer* IndexBuffer = QuadIndexBuffer.GetBuffer();
    DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    InstanceBuffer.Update(
        Context.Device,
        DeviceContext,
        Cmd.ParticleInstances,
        Cmd.ParticleInstanceCount);

    if (!InstanceBuffer.IsValid() || InstanceBuffer.GetInstanceCount() == 0)
    {
        return;
    }

    // b0(FrameBuffer)를 명시적으로 VS에 바인딩 — 빌보드 정렬에 View 행렬 필수.
    // Renderer::UpdateFrameBuffer가 frame 시작 시 전역 바인딩하지만, 직전 패스가
    // VS slot 0을 다른 cbuffer로 덮어쓸 위험이 있어 이 패스에서 한 번 더 못박는다.
    ID3D11Buffer* FrameBuf = Context.RenderResources->FrameBuffer.GetBuffer();
    if (FrameBuf)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &FrameBuf);
    }

    FSpriteParticleCB CBData = {};
    CBData.SubUVColumns = (Cmd.ParticleSubUVColumns > 0) ? Cmd.ParticleSubUVColumns : 1;
    CBData.SubUVRows    = (Cmd.ParticleSubUVRows    > 0) ? Cmd.ParticleSubUVRows    : 1;
    SpriteParticleCB.Update(DeviceContext, &CBData, sizeof(FSpriteParticleCB));
    ID3D11Buffer* CBBuf = SpriteParticleCB.GetBuffer();
    DeviceContext->VSSetConstantBuffers(8, 1, &CBBuf);

    Context.RenderResources->PerObjectConstantBuffer.Update(
        DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
    ID3D11Buffer* PerObjBuf = Context.RenderResources->PerObjectConstantBuffer.GetBuffer();
    DeviceContext->VSSetConstantBuffers(1, 1, &PerObjBuf);
    DeviceContext->PSSetConstantBuffers(1, 1, &PerObjBuf);

        ID3D11ShaderResourceView* TextureSRV = nullptr;
        if (Cmd.ParticleTexture)
        {
            TextureSRV = Cmd.ParticleTexture->GetSRV();
        }
        if (!TextureSRV)
        {
            TextureSRV = FResourceManager::Get().GetDefaultWhiteSRV();
        }
        DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);

    ID3D11Buffer* VBs[2] = { QuadVertexBuffer.GetBuffer(), InstanceBuffer.GetBuffer() };
    UINT Strides[2] = { QuadVertexBuffer.GetStride(), InstanceBuffer.GetStride() };
    UINT Offsets[2] = { 0, 0 };
    DeviceContext->IASetVertexBuffers(0, 2, VBs, Strides, Offsets);

    DeviceContext->DrawIndexedInstanced(6, InstanceBuffer.GetInstanceCount(), 0, 0, 0);
}

// Function : Render single Mesh particle emitter command (Cycle 11 옵션 B)
// input : Cmd, Context
// Cmd : render command produced by Builder Mesh case (MeshParticleInstances + MeshBuffer 세팅됨)
// Context : render pass context (Device, DeviceContext, RenderResources, ...)
// output : One DrawIndexedInstanced call issued when MeshBuffer + instance data valid
//
// D3D state: material blend + Default for opaque, DepthReadOnly for translucent + SolidBackCull.
// PerObject CB: Builder가 Identity Model로 세팅 — instance VB가 World 합성 담당.
// Slot 0: Cmd.MeshBuffer의 VertexBuffer (FNormalVertex), Slot 1: MeshInstanceBuffer (FMeshParticleInstanceData).
void FParticleRenderPass::RenderMeshEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    if (Cmd.MeshParticleInstances == nullptr || Cmd.MeshParticleInstanceCount == 0 || Cmd.MeshBuffer == nullptr)
    {
        return;
    }
    if (!Cmd.MeshBuffer->IsValid())
    {
        return;
    }

    FShaderProgram* Program = GetMeshParticleProgram();
    if (!Program || !Program->VS || !Program->PS)
    {
        return;
    }

    ID3D11DeviceContext* DeviceContext = Context.DeviceContext;

    Program->Bind(DeviceContext);

    const UMaterial* MeshMaterial = ResolveBaseMaterial(Cmd.Material);
    const EBlendType MeshBlendType = MeshMaterial ? MeshMaterial->BlendType : EBlendType::AlphaBlend;
    const EDepthStencilType MeshDepthType = (MeshBlendType == EBlendType::Opaque)
        ? EDepthStencilType::Default
        : EDepthStencilType::DepthReadOnly;

    // Mesh particles follow their material blend policy. Translucent materials depth-test but do not write depth,
    // matching the sprite/ribbon/beam particle pass behavior.
    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(MeshBlendType);
    DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(MeshDepthType);
    DeviceContext->OMSetDepthStencilState(DepthState, 0);
    ID3D11RasterizerState* RasterState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidBackCull);
    DeviceContext->RSSetState(RasterState);
    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);

    // Frame CB (View/Projection) — Sprite와 동일 안전 장치.
    ID3D11Buffer* FrameBuf = Context.RenderResources->FrameBuffer.GetBuffer();
    if (FrameBuf)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &FrameBuf);
    }

    // PerObject CB — Builder가 Identity Model로 세팅함 (instance VB가 World 합성 담당).
    Context.RenderResources->PerObjectConstantBuffer.Update(
        DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
    ID3D11Buffer* PerObjBuf = Context.RenderResources->PerObjectConstantBuffer.GetBuffer();
    DeviceContext->VSSetConstantBuffers(1, 1, &PerObjBuf);
    DeviceContext->PSSetConstantBuffers(1, 1, &PerObjBuf);

    // Albedo texture: Cmd.ParticleTexture (Builder가 Material.DiffuseMap에서 추출) 또는 default white.
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    if (Cmd.ParticleTexture)
    {
        TextureSRV = Cmd.ParticleTexture->GetSRV();
    }
    if (!TextureSRV)
    {
        TextureSRV = FResourceManager::Get().GetDefaultWhiteSRV();
    }
    DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);

    // Instance VB 업데이트 (grow-by-2x 자동).
    MeshInstanceBuffer.Update(
        Context.Device,
        DeviceContext,
        Cmd.MeshParticleInstances,
        Cmd.MeshParticleInstanceCount);
    if (!MeshInstanceBuffer.IsValid() || MeshInstanceBuffer.GetInstanceCount() == 0)
    {
        return;
    }

    // VB 바인딩: Slot 0 mesh per-vertex, Slot 1 per-instance.
    ID3D11Buffer* VBs[2] = {
        Cmd.MeshBuffer->GetVertexBuffer().GetBuffer(),
        MeshInstanceBuffer.GetBuffer()
    };
    UINT Strides[2] = {
        Cmd.MeshBuffer->GetVertexBuffer().GetStride(),
        MeshInstanceBuffer.GetStride()
    };
    UINT Offsets[2] = { 0, 0 };
    DeviceContext->IASetVertexBuffers(0, 2, VBs, Strides, Offsets);

    // IB 바인딩.
    ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
    DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // DrawIndexedInstanced: Builder가 SectionIndexCount + SectionIndexStart 세팅.
    // Cycle 11은 mesh 전체를 1 section처럼 (Start=0, Count=전체 index 수) 그리지만,
    // 추후 section 분할 draw가 필요해지면 Builder에서 Cmd 여러 개로 split 가능.
    DeviceContext->DrawIndexedInstanced(Cmd.SectionIndexCount, Cmd.MeshParticleInstanceCount, Cmd.SectionIndexStart, 0, 0);
}

// Function : Render single Ribbon particle emitter command (Cycle 12)
// input : Cmd, Context
// Cmd : render command produced by Builder Ribbon case (RibbonVertices + Material 세팅됨)
// Context : render pass context (Device, DeviceContext, RenderResources, ...)
// output : One Draw call issued when RibbonVertexBuffer valid (DrawIndexed 아님 — strip 은 index 불필요)
//
// D3D state: BlendAlpha + DepthReadOnly + SolidNoCull (사용자 결정 lock-in — ribbon trail 알파 마스킹).
// PerObject CB: Model 은 Identity (vertex 가 이미 world space — instance 도 없음).
// Slot 0: RibbonVertexBuffer (FRibbonParticleVertex), Slot 1: binding 없음.
// topology: TRIANGLESTRIP.
void FParticleRenderPass::RenderRibbonEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    if (Cmd.RibbonVertices == nullptr || Cmd.RibbonVertexCount == 0)
    {
        return;
    }

    FShaderProgram* Program = GetRibbonParticleProgram();
    if (!Program || !Program->VS || !Program->PS)
    {
        return;
    }

    ID3D11DeviceContext* DeviceContext = Context.DeviceContext;

    Program->Bind(DeviceContext);

    // 사용자 결정 lock-in: BlendAlpha + DepthReadOnly + SolidNoCull.
    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
    DeviceContext->OMSetDepthStencilState(DepthState, 0);
    ID3D11RasterizerState* RasterState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    DeviceContext->RSSetState(RasterState);
    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);

    // Frame CB (View/Projection).
    ID3D11Buffer* FrameBuf = Context.RenderResources->FrameBuffer.GetBuffer();
    if (FrameBuf)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &FrameBuf);
    }

    // PerObject CB — Builder가 Identity Model로 세팅 (vertex 가 이미 world space).
    Context.RenderResources->PerObjectConstantBuffer.Update(
        DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
    ID3D11Buffer* PerObjBuf = Context.RenderResources->PerObjectConstantBuffer.GetBuffer();
    DeviceContext->VSSetConstantBuffers(1, 1, &PerObjBuf);
    DeviceContext->PSSetConstantBuffers(1, 1, &PerObjBuf);

    // Albedo texture: Cmd.ParticleTexture (Builder가 Material.DiffuseMap에서 추출) 또는 default white.
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    if (Cmd.ParticleTexture)
    {
        TextureSRV = Cmd.ParticleTexture->GetSRV();
    }
    if (!TextureSRV)
    {
        TextureSRV = FResourceManager::Get().GetDefaultWhiteSRV();
    }
    DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);

    // Dynamic VB 업데이트 (grow-by-2x 자동).
    RibbonVertexBuffer.Update(
        Context.Device,
        DeviceContext,
        Cmd.RibbonVertices,
        Cmd.RibbonVertexCount);
    if (!RibbonVertexBuffer.IsValid() || RibbonVertexBuffer.GetInstanceCount() == 0)
    {
        return;
    }

    // VB 바인딩: Slot 0 only.
    ID3D11Buffer* VBs[1] = { RibbonVertexBuffer.GetBuffer() };
    UINT Strides[1] = { RibbonVertexBuffer.GetStride() };
    UINT Offsets[1] = { 0 };
    DeviceContext->IASetVertexBuffers(0, 1, VBs, Strides, Offsets);

    // topology = TRIANGLESTRIP. Begin 에서 TRIANGLELIST 로 세팅됐으므로 본 helper 에서 명시 override.
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Draw (indexless) — degenerate seam 으로 trail 사이 strip 연결 끊김.
    DeviceContext->Draw(RibbonVertexBuffer.GetInstanceCount(), 0);

    // TRIANGLELIST 로 복원 — 다음 helper (Sprite/Mesh) 가 TRIANGLELIST 가정.
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// Function : Render single Beam particle emitter command (Cycle 13a)
// input : Cmd, Context
// Cmd : render command produced by Builder Beam case (BeamVertices + Material 세팅됨)
// Context : render pass context (Device, DeviceContext, RenderResources, ...)
// output : One Draw call issued when BeamVertexBuffer valid (indexless — strip 은 index 불필요)
//
// D3D state: BlendAlpha + DepthReadOnly + SolidNoCull (Ribbon 와 동일 — Additive 본 cycle 외).
// PerObject CB: Model 은 Identity (vertex 가 이미 world space — instance 도 없음).
// Slot 0: BeamVertexBuffer (FBeamParticleVertex), Slot 1: binding 없음.
// topology: TRIANGLESTRIP. helper 끝에서 TRIANGLELIST 복원 (다음 Sprite/Mesh helper 보호).
void FParticleRenderPass::RenderBeamEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    if (Cmd.BeamVertices == nullptr || Cmd.BeamVertexCount == 0)
    {
        return;
    }

    FShaderProgram* Program = GetBeamParticleProgram();
    if (!Program || !Program->VS || !Program->PS)
    {
        return;
    }

    ID3D11DeviceContext* DeviceContext = Context.DeviceContext;

    Program->Bind(DeviceContext);

    // Ribbon 와 동일 lock-in state: BlendAlpha + DepthReadOnly + SolidNoCull.
    // EBlendType 에 Additive 값 없음 — RenderResources.h:66-71 (Opaque/AlphaBlend/NoColor 만).
    // Additive 도입은 Material 측 BlendType 시스템 별도 cycle.
    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
    DeviceContext->OMSetDepthStencilState(DepthState, 0);
    ID3D11RasterizerState* RasterState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    DeviceContext->RSSetState(RasterState);
    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);

    // Frame CB (View/Projection).
    ID3D11Buffer* FrameBuf = Context.RenderResources->FrameBuffer.GetBuffer();
    if (FrameBuf)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &FrameBuf);
    }

    // PerObject CB — Builder가 Identity Model로 세팅 (vertex 가 이미 world space).
    Context.RenderResources->PerObjectConstantBuffer.Update(
        DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
    ID3D11Buffer* PerObjBuf = Context.RenderResources->PerObjectConstantBuffer.GetBuffer();
    DeviceContext->VSSetConstantBuffers(1, 1, &PerObjBuf);
    DeviceContext->PSSetConstantBuffers(1, 1, &PerObjBuf);

    // Albedo texture: Cmd.ParticleTexture (Builder가 Material.DiffuseMap에서 추출) 또는 default white.
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    if (Cmd.ParticleTexture)
    {
        TextureSRV = Cmd.ParticleTexture->GetSRV();
    }
    if (!TextureSRV)
    {
        TextureSRV = FResourceManager::Get().GetDefaultWhiteSRV();
    }
    DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);

    // Dynamic VB 업데이트 (grow-by-2x 자동).
    BeamVertexBuffer.Update(
        Context.Device,
        DeviceContext,
        Cmd.BeamVertices,
        Cmd.BeamVertexCount);
    if (!BeamVertexBuffer.IsValid() || BeamVertexBuffer.GetInstanceCount() == 0)
    {
        return;
    }

    // VB 바인딩: Slot 0 only.
    ID3D11Buffer* VBs[1] = { BeamVertexBuffer.GetBuffer() };
    UINT Strides[1] = { BeamVertexBuffer.GetStride() };
    UINT Offsets[1] = { 0 };
    DeviceContext->IASetVertexBuffers(0, 1, VBs, Strides, Offsets);

    // topology = TRIANGLESTRIP. Begin 에서 TRIANGLELIST 로 세팅됐으므로 본 helper 에서 명시 override.
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Draw (indexless) — degenerate seam 으로 beam 사이 strip 연결 끊김.
    DeviceContext->Draw(BeamVertexBuffer.GetInstanceCount(), 0);

    // TRIANGLELIST 로 복원 — 다음 helper (Sprite/Mesh) 가 TRIANGLELIST 가정.
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool FParticleRenderPass::End(const FRenderPassContext* Context)
{

    return true;
}
