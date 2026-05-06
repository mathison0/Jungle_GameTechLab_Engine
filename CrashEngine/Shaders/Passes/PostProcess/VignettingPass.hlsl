#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

cbuffer VignetteParams : register(b2)
{
    float VignetteIntensity; 
    float VignetteRadius; 
    float VignetteSoftness; 
    float Padding;

    float3 VignetteColor;
    float bEnableVignette;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;
    
    float4 sceneColor = SceneColor.SampleLevel(LinearClampSampler, input.uv, 0);

    float2 centerUV = (uv - float2(0.5f, 0.5f)) * 2.0f;
    
    float distanceFromCenter = length(centerUV);
    
    float vignetteMask = smoothstep(
        VignetteRadius,
        VignetteRadius + VignetteSoftness,
        distanceFromCenter
    );
    
    vignetteMask *= VignetteIntensity;
    
    sceneColor.rgb = lerp(
        sceneColor.rgb,
        VignetteColor,
        vignetteMask
    );
    
    return sceneColor;
}