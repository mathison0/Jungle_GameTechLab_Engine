#include "../Common.hlsl"

Texture2D FinalSceneColor : register(t0);
SamplerState SampleState : register(s0);

cbuffer PostProcessBuffer : register(b10)
{
    float4 FadeColor;
    
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSoftness;
    float Padding3;
    
    float Gamma;
    float LetterBoxRatio;
    float2 InvResolution;
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
    int2 ip = int2(input.ClipPos.xy);
    float4 color = FinalSceneColor.Load(int3(ip, 0));
    float2 uv = (float2(ip) + 0.5f) * InvResolution;
    
    // Letterbox
    float ratio = saturate(LetterBoxRatio);
    if (uv.y < ratio || uv.y > 1.0f - ratio)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    // Vignetting
    float2 distVec = uv - 0.5f; // 중앙까지의 거리 벡터
    float dist = length(distVec);
    float vignette = smoothstep(VignetteRadius, VignetteRadius - VignetteSoftness, dist);
    color.rgb *= lerp(1.0f, vignette, VignetteIntensity);

    // Fade In/Out
    float fadeAlpha = saturate(FadeColor.a);
    color.rgb = lerp(color.rgb, FadeColor.rgb, fadeAlpha);
    
    // Gamma Correction: 항상 최종 단계에 수행
    color.rgb = pow(abs(color.rgb), 1.0f / max(Gamma, 0.01f));
    color.a = 1.0f;
    
    return color;
}
