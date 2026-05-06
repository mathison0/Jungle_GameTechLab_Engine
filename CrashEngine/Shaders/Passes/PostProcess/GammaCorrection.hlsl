#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

cbuffer GammaCorrectionParams : register(b2)
{
    float DisplayGamma;
    float3 Padding;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float3 ApplyGammaCorrection(float3 linearColor)
{
    linearColor = saturate(linearColor);
    return pow(linearColor, 1.0f / max(DisplayGamma, 0.0001f));
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColor.SampleLevel(LinearClampSampler, input.uv, 0);
    float3 corrected = ApplyGammaCorrection(sceneColor.rgb);

    return float4(corrected, 1.0f);
}
