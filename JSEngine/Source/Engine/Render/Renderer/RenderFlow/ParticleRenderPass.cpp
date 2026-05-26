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

    bGPUResourcesReady = QuadVertexBuffer.GetBuffer() != nullptr
        && QuadIndexBuffer.GetBuffer() != nullptr
        && InstanceBuffer.IsValid()
        && SpriteParticleCB.GetBuffer() != nullptr;
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
    // 본 cycle은 Sprite만 본문 보유, Mesh/Ribbon/Beam은 NOP. Cycle 11+에서 본문 채움.
    // 단일 Pass + procedural switch 구조 (사용자 결정 3).
    bool bAnySpriteRendered = false;
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
    // Sprite Cmd가 한 번이라도 처리됐을 때만 unbind 필요 (다른 type은 slot 1 사용 안 함).
    if (bAnySpriteRendered)
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

// Function : Render single Mesh particle emitter command (Cycle 10a NOP)
// input : Cmd, Context (unused in NOP)
// output : None — Cycle 11에서 본문 채움
void FParticleRenderPass::RenderMeshEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    (void)Cmd;
    (void)Context;
    // Cycle 11 (Mesh emitter)에서 본문 채움.
}

// Function : Render single Ribbon particle emitter command (Cycle 10a NOP)
// input : Cmd, Context (unused in NOP)
// output : None — Cycle 12b에서 본문 채움
void FParticleRenderPass::RenderRibbonEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    (void)Cmd;
    (void)Context;
    // Cycle 12b (Ribbon render)에서 본문 채움.
}

// Function : Render single Beam particle emitter command (Cycle 10a NOP)
// input : Cmd, Context (unused in NOP)
// output : None — Cycle 13b에서 본문 채움
void FParticleRenderPass::RenderBeamEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context)
{
    (void)Cmd;
    (void)Context;
    // Cycle 13b (Beam render)에서 본문 채움.
}

bool FParticleRenderPass::End(const FRenderPassContext* Context)
{

    return true;
}
