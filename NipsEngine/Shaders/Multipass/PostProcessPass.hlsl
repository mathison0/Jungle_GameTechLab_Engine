#include "../Common.hlsl"

cbuffer PostProcessBuffer : register(b0)
{
    float4 FadeColor;
    
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSoftness;
    float Padding3;
    
    float Gamma;
    float LetterBoxRatio;
    float2 Padding4;
};

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 pos;
    if (vertexID == 0)
        pos = float2(-1.0f, -1.0f);
    else if (vertexID == 1)
        pos = float2(-1.0f, 3.0f);
    else
        pos = float2(3.0f, -1.0f);

    output.ClipPos = float4(pos, 0.0f, 1.0f);
    return output;
}


float4 mainPS(VSOutput input) : SV_TARGET
{
    return input;
}
