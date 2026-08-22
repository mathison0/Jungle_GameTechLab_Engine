#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/SystemResources.hlsli"

Texture2D MaskTexture : register(t0);
Texture2D NoiseTexture : register(t6);

cbuffer SlashRefractionParams : register(b2)
{
    float SlashAlpha;
    float RefractionStrength;
    float RefractionNoiseTiling;
    float RefractionNoiseScroll;

    float RefractionEdgeBoost;
    float RefractionPad0;
    float2 RefractionDirection;

    float SlashReveal;
    float SlashRevealSoftness;
    float SlashEdgeSoftness;
    float SlashTailFadeStart;
    float SlashTrailLength;
    float SlashScreenThickness;
    float SlashScreenThicknessStrength;
    float2 SlashRefractionPad;
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

float GetSlashMask(float4 maskTex)
{
    // alpha가 넓은 회색 면이면 rgb luminance 쪽이 더 나을 수 있음.
    float rgbMask = max(maskTex.r, max(maskTex.g, maskTex.b));
    float alphaMask = maskTex.a;

    return saturate(max(rgbMask, alphaMask));
}

float GetSlashShapeMask(float2 uv)
{
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

    return saturate(leadingMask * trailingMask * edgeMask * tailMask);
}

float4 PS(PS_Input_Particle input) : SV_TARGET
{
    float2 uv = input.texcoord;

    uint sceneWidth;
    uint sceneHeight;
    SceneColorTexture.GetDimensions(sceneWidth, sceneHeight);

    float2 screenUV = input.position.xy / float2(sceneWidth, sceneHeight);
    screenUV = saturate(screenUV);

    float2 noiseUV1 = uv * RefractionNoiseTiling;
    noiseUV1.x += RefractionNoiseScroll;

    float2 noiseUV2 = uv.yx * (RefractionNoiseTiling * 1.73f);
    noiseUV2.y -= RefractionNoiseScroll * 0.7f;
    noiseUV2 += float2(0.37f, 0.19f);

    float2 n1 = NoiseTexture.Sample(LinearWrapSampler, noiseUV1).rg * 2.0f - 1.0f;
    float2 n2 = NoiseTexture.Sample(LinearWrapSampler, noiseUV2).rg * 2.0f - 1.0f;

    float2 noiseWarp = normalize(n1 + n2 * 0.65f + 0.001f);

    float2 dir = RefractionDirection;
    if (dot(dir, dir) < 0.0001f)
    {
        dir = float2(0.0f, 1.0f);
    }
    dir = normalize(dir);

    // 텍스처 마스크 대신 UV 기반 소프트 마스크.
    // 메쉬 전체는 살리고, UV 가장자리만 살짝 부드럽게 줄임.
    float maskTex = GetSlashMask(MaskTexture.Sample(LinearWrapSampler, uv));
    float softMeshMask = saturate(GetSlashShapeMask(uv) * maskTex * SlashAlpha);

    // 일단 확실히 보이도록 강도 마스크를 넓게 준다.
    float ridge = 1.0f;
    float strengthMask = softMeshMask;

    float mask = softMeshMask;

    float2 pixelOffset = float2(0.0f, 50.0f / max(sceneHeight, 1));
    float2 strongOffset = pixelOffset * mask;

    float4 original = SceneColorTexture.SampleLevel(
        LinearClampSampler,
        screenUV,
        0
    );

    float r = SceneColorTexture.SampleLevel(
        LinearClampSampler,
        saturate(screenUV + strongOffset * 1.04f),
        0
    ).r;

    float g = SceneColorTexture.SampleLevel(
        LinearClampSampler,
        saturate(screenUV + strongOffset),
        0
    ).g;

    float b = SceneColorTexture.SampleLevel(
        LinearClampSampler,
        saturate(screenUV + strongOffset * 0.96f),
        0
    ).b;

    float4 refracted = float4(r, g, b, 1.0f);

    float amount = saturate(mask * 0.55f);
    float4 scene = lerp(original, refracted, amount);

    scene.rgb += mask * RefractionEdgeBoost * 0.025f;

    return scene;
}
