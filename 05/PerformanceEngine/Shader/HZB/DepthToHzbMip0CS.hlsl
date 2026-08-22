cbuffer HzbBuildCB : register(b0)
{
    uint SourceMip;
    uint OutputWidth;
    uint OutputHeight;
    uint Padding;
};

Texture2D<float> DepthTexture : register(t0);
RWTexture2D<float> HzbMipOut : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    if (DispatchThreadID.x >= OutputWidth || DispatchThreadID.y >= OutputHeight)
    {
        return;
    }

    HzbMipOut[DispatchThreadID.xy] = DepthTexture.Load(int3(DispatchThreadID.xy, 0));
}
