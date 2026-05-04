#include "../../Resources/SystemSamplers.hlsl"

struct VS_Input_UI
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

struct PS_Input_UI
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

cbuffer UIParams : register(b2)
{
    float4 UIFlags;
};

Texture2D UITexture : register(t0);

PS_Input_UI VS(VS_Input_UI input)
{
    PS_Input_UI output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_UI input) : SV_TARGET
{
    if (UIFlags.x > 0.5f)
    {
        return UITexture.Sample(LinearClampSampler, input.uv) * input.color;
    }

    return input.color;
}
