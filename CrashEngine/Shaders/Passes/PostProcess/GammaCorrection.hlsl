#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

static const float DisplayGamma = 2.2f;

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float3 ApplyGammaCorrection(float3 linearColor)
{
    linearColor = saturate(linearColor);
    return pow(linearColor, 1.0f / DisplayGamma);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColor.SampleLevel(LinearClampSampler, input.uv, 0);
    float3 corrected = ApplyGammaCorrection(sceneColor.rgb);

    return float4(corrected, 1.0f);
}