#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/Fog.hlsli"
#include "Common/MaterialBloom.hlsli"

Texture2D DiffuseTexture : register(t0);
Texture2D NoiseTexture : register(t6);

cbuffer SlashParams : register(b2)
{
    float SlashAlpha;
    float SlashColorStrength;
    float SlashDissolve;
    float SlashDissolveSoftness;

    float4 SlashTint;

    float SlashNoiseTiling;
    float SlashNoiseScroll;
    float2 SlashPad;

    float SlashReveal;
    float SlashRevealSoftness;
    float SlashEdgeSoftness;
    float SlashTailFadeStart;
    float SlashTrailLength;
};

PS_Input_Particle VS(VS_Input_PNCTT input)
{
    PS_Input_Particle output;

    float4 worldPos = mul(float4(input.position, 1.0f), Model);

    output.worldPos = worldPos.xyz;
    output.position = mul(mul(worldPos, View), Projection);
    output.color = input.color;
    output.texcoord = input.texcoord;

    return output;
}

float4 PS(PS_Input_Particle input) : SV_TARGET
{
    float2 uv = input.texcoord;

    float4 tex = DiffuseTexture.Sample(LinearWrapSampler, uv);

    float2 noiseUV = uv * SlashNoiseTiling;
    noiseUV.x += SlashNoiseScroll;

    float noise = NoiseTexture.Sample(LinearWrapSampler, noiseUV).r;

    float dissolve = smoothstep(
        SlashDissolve,
        SlashDissolve + max(SlashDissolveSoftness, 0.001f),
        noise
    );

    float revealSoftness = max(SlashRevealSoftness, 0.001f);
    float revealHead = saturate(SlashReveal);
    float leadingMask = 1.0f - smoothstep(revealHead, revealHead + revealSoftness, uv.x);

    float trailingMask = 1.0f;
    float trailLength = saturate(SlashTrailLength);
    if (trailLength < 0.999f)
    {
        float trailStart = revealHead - trailLength;
        trailingMask = smoothstep(trailStart, trailStart + revealSoftness, uv.x);
    }

    float edgeSoftness = max(SlashEdgeSoftness, 0.001f);
    float edgeMask = smoothstep(0.0f, edgeSoftness, uv.y)
        * (1.0f - smoothstep(1.0f - edgeSoftness, 1.0f, uv.y));

    float tailMask = 1.0f;
    if (SlashTailFadeStart < 0.999f)
    {
        tailMask = 1.0f - smoothstep(SlashTailFadeStart, 1.0f, uv.x);
    }

    float shapeMask = saturate(leadingMask * trailingMask * edgeMask * tailMask);

    float4 color = tex * input.color;
    color.rgb *= SlashTint.rgb * SlashColorStrength;
    color.a *= SlashAlpha * SlashTint.a * dissolve * shapeMask;

    color.rgb = ApplyHeightFog(color.rgb, input.worldPos);
    color.rgb = ApplyMaterialEmissive(color.rgb);

    return color;
}
