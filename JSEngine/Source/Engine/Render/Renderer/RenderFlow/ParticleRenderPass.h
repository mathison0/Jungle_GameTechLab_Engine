#pragma once
#include "RenderPass.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/InstanceBuffer.h"

class FParticleRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;

    bool EnsureGPUResources(ID3D11Device* Device);

    FVertexBuffer   QuadVertexBuffer;
    FIndexBuffer    QuadIndexBuffer;
    FInstanceBuffer InstanceBuffer;
    FConstantBuffer SpriteParticleCB;   // b8: SubUV grid
    bool bGPUResourcesReady = false;
};
