#include "Common.hlsl"

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float4 Tangent : TEXCOORD2;
    float2 UV : TEXCOORD3;
};

struct FAmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct FDirectionalLightInfo
{
    float3 Direction;
    float Intensity;

    float3 Color;
    float padding;
};

struct FPointLightInfo
{
    float3 Position;
    float Radius;

    float3 Color;
    float Intensity;
};

struct FSpotLightInfo
{
    float3 Position;
    float Radius;

    float3 Color;
    float Intensity;
    
    float3 Direction;
    float Angle;
    
    float3 Padding;
};

cbuffer Lighting : register(b15)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional;
};

StructuredBuffer<FPointLightInfo> PointLights : register(t0);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t1);
StructuredBuffer<uint> TilePointLightIndices : register(t2);
StructuredBuffer<uint> TileSpotLightIndices : register(t3);
StructuredBuffer<uint2> TilePointLightGrid : register(t4);
StructuredBuffer<uint2> TileSpotLightGrid : register(t5);


PSInput VS(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.Position, 1.0f), Model);

    output.WorldPos = worldPos.xyz;
    float4 viewPos = mul(worldPos, View);
    output.ClipPos = mul(viewPos, Projection);

    output.WorldNormal = normalize(mul(input.Normal, (float3x3) Model));

    output.UV = input.UV;

    output.Tangent = float4(0, 0, 0, 1);

    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 output;
    
    return output;
}
