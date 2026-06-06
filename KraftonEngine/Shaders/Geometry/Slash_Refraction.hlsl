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

float GetSlashMask(float4 maskTex)
{
    // alpha가 넓은 회색 면이면 rgb luminance 쪽이 더 나을 수 있음.
    float rgbMask = max(maskTex.r, max(maskTex.g, maskTex.b));
    float alphaMask = maskTex.a;

    return saturate(max(rgbMask, alphaMask));
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
        dir = float2(1.0f, 0.18f);
    }
    dir = normalize(dir);

    // 텍스처 마스크 대신 UV 기반 소프트 마스크.
    // 메쉬 전체는 살리고, UV 가장자리만 살짝 부드럽게 줄임.
    float edgeX = smoothstep(0.00f, 0.08f, uv.x) * (1.0f - smoothstep(0.92f, 1.00f, uv.x));
    float edgeY = smoothstep(0.00f, 0.08f, uv.y) * (1.0f - smoothstep(0.92f, 1.00f, uv.y));
    float softMeshMask = saturate(edgeX * edgeY);

    // 일단 확실히 보이도록 강도 마스크를 넓게 준다.
    float ridge = 1.0f;
    float strengthMask = softMeshMask;

    float2 offset =
        noiseWarp * RefractionStrength * strengthMask
        + dir * RefractionStrength * ridge * 0.45f * strengthMask;

    offset = clamp(offset, -0.10f, 0.10f);

    float4 original = SceneColorTexture.SampleLevel(
        LinearClampSampler,
        screenUV,
        0
    );

    float4 refracted = SceneColorTexture.SampleLevel(
        LinearClampSampler,
        saturate(screenUV + offset),
        0
    );

    // 최소 0.75 이상 섞어서 테스트 단계에서 반드시 티 나게 함.
    float refractionAmount = saturate(softMeshMask * 0.9f);
    float4 scene = lerp(original, refracted, refractionAmount);

    scene.rgb += softMeshMask * RefractionEdgeBoost * 0.015f;

    return scene;
}
