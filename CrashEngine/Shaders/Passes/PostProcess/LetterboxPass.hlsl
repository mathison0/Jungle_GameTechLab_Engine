#include "../../Utils/Functions.hlsl"
#include "../../Resources/SystemResources.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

cbuffer LetterboxParams : register(b2)
{
    float TargetAspectRatio;
    float ViewportAspectRatio;
    float LetterboxOpacity;
    float Padding0;

    float3 LetterboxColor;
    float Padding1;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float ComputeLetterboxMask(float2 uv)
{
    float currentAspect = max(ViewportAspectRatio, 0.0001f);
    float targetAspect = max(TargetAspectRatio, 0.0001f);

    if (currentAspect < targetAspect)
    {
        float activeHeight = saturate(currentAspect / targetAspect);
        float barSize = (1.0f - activeHeight) * 0.5f;
        return saturate(step(uv.y, barSize) + step(1.0f - barSize, uv.y));
    }

    if (currentAspect > targetAspect)
    {
        float activeWidth = saturate(targetAspect / currentAspect);
        float barSize = (1.0f - activeWidth) * 0.5f;
        return saturate(step(uv.x, barSize) + step(1.0f - barSize, uv.x));
    }

    return 0.0f;
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 sceneColor = SceneColor.SampleLevel(LinearClampSampler, input.uv, 0);
    float mask = ComputeLetterboxMask(input.uv) * saturate(LetterboxOpacity);
    sceneColor.rgb = lerp(sceneColor.rgb, LetterboxColor, mask);

    return sceneColor;
}
