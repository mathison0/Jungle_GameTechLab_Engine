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

    float4 color = tex * input.color;
    color.rgb *= SlashTint.rgb * SlashColorStrength;
    color.a *= SlashAlpha * SlashTint.a * dissolve;

    color.rgb = ApplyHeightFog(color.rgb, input.worldPos);
    color.rgb = ApplyMaterialEmissive(color.rgb);

    return color;
}
