#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

cbuffer FadeParams : register(b2)
{
    float3 FadeColor;
    float FadeAlpha;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColor.SampleLevel(LinearClampSampler, input.uv, 0);
    sceneColor.rgb = lerp(sceneColor.rgb, FadeColor, saturate(FadeAlpha));

    return sceneColor;
}
