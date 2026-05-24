#include "ParticleRenderPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/Resource/Texture.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Scene/RenderCommand.h"

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

    FShaderProgram* Program = GetSpriteParticleProgram();
    if (!Program || !Program->VS || !Program->PS)
    {
        return false;
    }

    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

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

    for (const FRenderCommand& Cmd : Commands)
    {
        if (Cmd.ParticleInstances == nullptr || Cmd.ParticleInstanceCount == 0)
        {
            continue;
        }

        InstanceBuffer.Update(
            Context->Device,
            DeviceContext,
            Cmd.ParticleInstances,
            Cmd.ParticleInstanceCount);

        if (!InstanceBuffer.IsValid() || InstanceBuffer.GetInstanceCount() == 0)
        {
            continue;
        }

        FSpriteParticleCB CBData = {};
        CBData.SubUVColumns = (Cmd.ParticleSubUVColumns > 0) ? Cmd.ParticleSubUVColumns : 1;
        CBData.SubUVRows    = (Cmd.ParticleSubUVRows    > 0) ? Cmd.ParticleSubUVRows    : 1;
        SpriteParticleCB.Update(DeviceContext, &CBData, sizeof(FSpriteParticleCB));
        ID3D11Buffer* CBBuf = SpriteParticleCB.GetBuffer();
        DeviceContext->VSSetConstantBuffers(8, 1, &CBBuf);

        Context->RenderResources->PerObjectConstantBuffer.Update(
            DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
        ID3D11Buffer* PerObjBuf = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
        DeviceContext->VSSetConstantBuffers(1, 1, &PerObjBuf);
        DeviceContext->PSSetConstantBuffers(1, 1, &PerObjBuf);

        ID3D11ShaderResourceView* TextureSRV = nullptr;
        if (Cmd.ParticleTexture)
        {
            TextureSRV = Cmd.ParticleTexture->GetSRV();
        }
        DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);

        ID3D11Buffer* VBs[2] = { QuadVertexBuffer.GetBuffer(), InstanceBuffer.GetBuffer() };
        UINT Strides[2] = { QuadVertexBuffer.GetStride(), InstanceBuffer.GetStride() };
        UINT Offsets[2] = { 0, 0 };
        DeviceContext->IASetVertexBuffers(0, 2, VBs, Strides, Offsets);

        DeviceContext->DrawIndexedInstanced(6, InstanceBuffer.GetInstanceCount(), 0, 0, 0);
    }


	//TODO
	//Slot 해제는 End logic에서 담당하도록 수정하는게 가독성에 유리함. 추후 진행
	// Slot 1을 다른 패스가 자동으로 미사용한다고 가정해도 위험. instance VB는 binding 해제.
    ID3D11Buffer* NullBuffer = nullptr;
    UINT NullStride = 0;
    UINT NullOffset = 0;
    DeviceContext->IASetVertexBuffers(1, 1, &NullBuffer, &NullStride, &NullOffset);

    return true;
}

bool FParticleRenderPass::End(const FRenderPassContext* Context)
{

    return true;
}
