cbuffer HzbBuildCB : register(b0)
{
    uint SourceMip;
    uint OutputWidth;
    uint OutputHeight;
    uint Padding;
};

Texture2D<float> SourceHzb : register(t0);
RWTexture2D<float> DestHzbMip : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    if (DispatchThreadID.x >= OutputWidth || DispatchThreadID.y >= OutputHeight)
    {
        return;
    }

    uint SourceWidth = 0;
    uint SourceHeight = 0;
    uint SourceLevels = 0;
    SourceHzb.GetDimensions(0, SourceWidth, SourceHeight, SourceLevels);

    uint2 SourceBase = DispatchThreadID.xy * 2;
    uint2 MaxCoord = uint2(SourceWidth - 1, SourceHeight - 1);
    uint2 Coord00 = min(SourceBase, MaxCoord);
    uint2 Coord10 = min(SourceBase + uint2(1, 0), MaxCoord);
    uint2 Coord01 = min(SourceBase + uint2(0, 1), MaxCoord);
    uint2 Coord11 = min(SourceBase + uint2(1, 1), MaxCoord);

    const int Mip = 0;
    float Depth00 = SourceHzb.Load(int3(Coord00, Mip));
    float Depth10 = SourceHzb.Load(int3(Coord10, Mip));
    float Depth01 = SourceHzb.Load(int3(Coord01, Mip));
    float Depth11 = SourceHzb.Load(int3(Coord11, Mip));

    DestHzbMip[DispatchThreadID.xy] = max(max(Depth00, Depth10), max(Depth01, Depth11));
}
