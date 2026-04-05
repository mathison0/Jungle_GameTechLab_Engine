static const float DepthClearValue = 1.0f;
static const float SmallClipW = 1.0e-6f;
static const float MinViewDepthEpsilon = 1.0e-3f;

struct FGpuOcclusionCandidate
{
    float3 BoundsMin;
    float Padding0;
    float3 BoundsMax;
    float Padding1;
};

cbuffer OcclusionCullCB : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 ViewProjection;
    uint CandidateCount;
    uint MipCount;
    uint DepthWidth;
    uint DepthHeight;
    float NearClip;
    float DepthEpsilon;
    float2 Padding;
};

StructuredBuffer<FGpuOcclusionCandidate> Candidates : register(t0);
Texture2D<float> HzbTexture : register(t1);
RWStructuredBuffer<uint> VisibilityFlags : register(u0);

float3 BuildCorner(FGpuOcclusionCandidate Candidate, uint CornerIndex)
{
    return float3(
        (CornerIndex & 1u) != 0u ? Candidate.BoundsMax.x : Candidate.BoundsMin.x,
        (CornerIndex & 2u) != 0u ? Candidate.BoundsMax.y : Candidate.BoundsMin.y,
        (CornerIndex & 4u) != 0u ? Candidate.BoundsMax.z : Candidate.BoundsMin.z);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    const uint CandidateIndex = DispatchThreadID.x;
    if (CandidateIndex >= CandidateCount)
    {
        return;
    }

    FGpuOcclusionCandidate Candidate = Candidates[CandidateIndex];

    float MinScreenX = 3.402823466e+38f;
    float MinScreenY = 3.402823466e+38f;
    float MaxScreenX = -3.402823466e+38f;
    float MaxScreenY = -3.402823466e+38f;
    float MinDepth = DepthClearValue;
    bool HasProjectedVertex = false;

    [unroll]
    for (uint CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        float3 Corner = BuildCorner(Candidate, CornerIndex);
        float4 ViewSpace = mul(float4(Corner, 1.0f), View);
        if (ViewSpace.z <= (NearClip + MinViewDepthEpsilon))
        {
            VisibilityFlags[CandidateIndex] = 1u;
            return;
        }

        float4 ClipSpace = mul(float4(Corner, 1.0f), ViewProjection);
        if (ClipSpace.w <= SmallClipW)
        {
            VisibilityFlags[CandidateIndex] = 1u;
            return;
        }

        float3 Ndc = ClipSpace.xyz / ClipSpace.w;
        float ScreenX = ((Ndc.x * 0.5f) + 0.5f) * (float)DepthWidth;
        float ScreenY = (1.0f - ((Ndc.y * 0.5f) + 0.5f)) * (float)DepthHeight;

        MinScreenX = min(MinScreenX, ScreenX);
        MinScreenY = min(MinScreenY, ScreenY);
        MaxScreenX = max(MaxScreenX, ScreenX);
        MaxScreenY = max(MaxScreenY, ScreenY);
        MinDepth = min(MinDepth, saturate(Ndc.z));
        HasProjectedVertex = true;
    }

    if (!HasProjectedVertex)
    {
        VisibilityFlags[CandidateIndex] = 1u;
        return;
    }

    int MinX = clamp((int)floor(MinScreenX), 0, (int)DepthWidth - 1);
    int MinY = clamp((int)floor(MinScreenY), 0, (int)DepthHeight - 1);
    int MaxX = clamp((int)ceil(MaxScreenX) - 1, 0, (int)DepthWidth - 1);
    int MaxY = clamp((int)ceil(MaxScreenY) - 1, 0, (int)DepthHeight - 1);
    if (MinX > MaxX || MinY > MaxY)
    {
        VisibilityFlags[CandidateIndex] = 1u;
        return;
    }

    uint Width = (uint)(MaxX - MinX + 1);
    uint Height = (uint)(MaxY - MinY + 1);
    if (max(Width, Height) < 2u)
    {
        VisibilityFlags[CandidateIndex] = 1u;
        return;
    }

    uint SelectedMip = 0u;
    while ((SelectedMip + 1u) < MipCount)
    {
        uint NextMip = SelectedMip + 1u;
        uint NextMipWidth = 0u;
        uint NextMipHeight = 0u;
        uint NextMipLevels = 0u;
        HzbTexture.GetDimensions(NextMip, NextMipWidth, NextMipHeight, NextMipLevels);

        uint NextMinX = min((uint)MinX >> NextMip, NextMipWidth - 1u);
        uint NextMinY = min((uint)MinY >> NextMip, NextMipHeight - 1u);
        uint NextMaxX = min((uint)MaxX >> NextMip, NextMipWidth - 1u);
        uint NextMaxY = min((uint)MaxY >> NextMip, NextMipHeight - 1u);
        uint FootprintWidth = (NextMaxX - NextMinX) + 1u;
        uint FootprintHeight = (NextMaxY - NextMinY) + 1u;

        SelectedMip = NextMip;
        if (FootprintWidth <= 2u && FootprintHeight <= 2u)
        {
            break;
        }
    }

    uint SelectedMipWidth = 0;
    uint SelectedMipHeight = 0;
    uint SelectedMipLevels = 0;
    HzbTexture.GetDimensions(SelectedMip, SelectedMipWidth, SelectedMipHeight, SelectedMipLevels);

    uint MipRectMinX = min((uint)MinX >> SelectedMip, SelectedMipWidth - 1u);
    uint MipRectMinY = min((uint)MinY >> SelectedMip, SelectedMipHeight - 1u);
    uint MipRectMaxX = min((uint)MaxX >> SelectedMip, SelectedMipWidth - 1u);
    uint MipRectMaxY = min((uint)MaxY >> SelectedMip, SelectedMipHeight - 1u);

    int Mip = (int)SelectedMip;
    float MaxHzbDepth = 0.0f;
    [loop]
    for (uint Y = MipRectMinY; Y <= MipRectMaxY; ++Y)
    {
        [loop]
        for (uint X = MipRectMinX; X <= MipRectMaxX; ++X)
        {
            MaxHzbDepth = max(MaxHzbDepth, HzbTexture.Load(int3(X, Y, Mip)));
        }
    }

    VisibilityFlags[CandidateIndex] = (MinDepth > (MaxHzbDepth + DepthEpsilon)) ? 0u : 1u;
}
