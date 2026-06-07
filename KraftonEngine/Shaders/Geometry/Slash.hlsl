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
    float4 SlashDissolveEdgeColor;

    float SlashNoiseTiling;
    float SlashNoiseScroll;
    float SlashScreenThickness;
    float SlashScreenThicknessStrength;

    float SlashReveal;
    float SlashRevealSoftness;
    float SlashEdgeSoftness;
    float SlashTailFadeStart;
    float SlashTrailLength;
    float SlashDissolveEdgeWidth;
    float SlashDissolveEdgeIntensity;
    float2 SlashDissolveEdgePad;
};

float4 ApplySlashScreenThickness(VS_Input_PNCTT input, float4 worldPos, float4 clipPos)
{
    float strength = max(SlashScreenThicknessStrength, 0.0f);
    float thickness = max(SlashScreenThickness, 0.0f) * strength;
    if (thickness <= 0.0f)
    {
        return clipPos;
    }

    float3 worldTangent = mul(float4(input.tangent.xyz, 0.0f), Model).xyz;
    if (dot(worldTangent, worldTangent) <= 0.000001f)
    {
        return clipPos;
    }

    worldTangent = normalize(worldTangent);

    float4 tangentClip = mul(mul(float4(worldPos.xyz + worldTangent, 1.0f), View), Projection);
    float clipW = abs(clipPos.w) > 0.00001f ? clipPos.w : 0.00001f;
    float tangentW = abs(tangentClip.w) > 0.00001f ? tangentClip.w : 0.00001f;
    float2 screenTangent = tangentClip.xy / tangentW - clipPos.xy / clipW;

    if (dot(screenTangent, screenTangent) <= 0.00000001f)
    {
        return clipPos;
    }

    float2 screenNormal = normalize(float2(-screenTangent.y, screenTangent.x));
    float2 viewportSize = max(ViewportSize, float2(1.0f, 1.0f));
    float2 ndcPerPixel = 2.0f / viewportSize;
    float side = input.texcoord.y * 2.0f - 1.0f;

    clipPos.xy += screenNormal * ndcPerPixel * thickness * side * clipPos.w;
    return clipPos;
}

PS_Input_Particle VS(VS_Input_PNCTT input)
{
    PS_Input_Particle output;

    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    float4 clipPos = mul(mul(worldPos, View), Projection);

    output.worldPos = worldPos.xyz;
    output.position = ApplySlashScreenThickness(input, worldPos, clipPos);
    output.color = input.color;
    output.texcoord = input.texcoord;

    return output;
}

float4 PS(PS_Input_Particle input) : SV_TARGET
{
    float2 uv = input.texcoord;

    float4 tex = DiffuseTexture.Sample(LinearWrapSampler, uv);

    float2 noiseUV1 = uv * SlashNoiseTiling;
    noiseUV1.x += SlashNoiseScroll;

    float2 noiseUV2 = uv.yx * (SlashNoiseTiling * 2.7f);
    noiseUV2.y -= SlashNoiseScroll * 0.65f;
    noiseUV2 += float2(0.31f, 0.17f);

    float noiseA = NoiseTexture.Sample(LinearWrapSampler, noiseUV1).r;
    float noiseB = NoiseTexture.Sample(LinearWrapSampler, noiseUV2).r;

    float noise = saturate(noiseA * 0.65f + noiseB * 0.35f);

    float dissolve = smoothstep(
        SlashDissolve,
        SlashDissolve + max(SlashDissolveSoftness, 0.001f),
        noise
    );

    float edgeWidth = max(SlashDissolveEdgeWidth, 0.001f);
    float edgeActivation = saturate(SlashDissolve / edgeWidth);
    float dissolveEdge =
        smoothstep(SlashDissolve, SlashDissolve + edgeWidth, noise)
        * (1.0f - smoothstep(SlashDissolve + edgeWidth, SlashDissolve + edgeWidth * 2.0f, noise))
        * edgeActivation;

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
    color.rgb += SlashDissolveEdgeColor.rgb
        * SlashDissolveEdgeIntensity
        * dissolveEdge
        * shapeMask;
    color.a *= SlashAlpha * SlashTint.a * dissolve * shapeMask;

    color.rgb = ApplyHeightFog(color.rgb, input.worldPos);
    color.rgb = ApplyMaterialEmissive(color.rgb);

    return color;
}
