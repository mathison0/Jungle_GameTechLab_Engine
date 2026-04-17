#include "Common.hlsl"

// StaticMesh Material (b6)
cbuffer StaticMeshBuffer : register(b6)
{
    float3 AmbientColor;    // Ka
    float3 DiffuseColor;    // Kd
    float3 SpecularColor;   // Ks
    float  Shininess;       // Ns
    // Camera
    float3 CameraWorldPos;
    // ScrollUV
    float2 ScrollUV;
    float  Padding6_1;
    uint   bHasDiffuseMap;
    uint   bHasSpecularMap;
    float  Padding6_2;
    float  Padding6_3;
};

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

// Lighting (b13)
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

cbuffer Lighting : register(b13)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional;
};

// Light Data (t0-t5)
StructuredBuffer<FPointLightInfo> PointLights : register(t0);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t1);
StructuredBuffer<uint> TilePointLightIndices : register(t2);
StructuredBuffer<uint> TileSpotLightIndices : register(t3);
StructuredBuffer<uint2> TilePointLightGrid : register(t4);
StructuredBuffer<uint2> TileSpotLightGrid : register(t5);

// StaticMesh Textures (t6-t9)
Texture2D DiffuseMap : register(t6);
Texture2D AmbientMap : register(t7);
Texture2D SpecularMap : register(t8);
Texture2D BumpMap : register(t9);

SamplerState SampleState : register(s0);

PSInput VS(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    output.WorldPos = worldPos.xyz;
    output.ClipPos = mul(mul(worldPos, View), Projection);
    
    // 비균일 스케일을 위한 역행렬 이후 전치 
    // 역행렬은 비용이 많이 들어서 상수 버퍼로 가져오는 게 나을 거 같네요...
    float3x3 normalMatrix = transpose(Inverse3x3((float3x3) Model));
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) Model));

    output.UV = input.UV + ScrollUV;

    output.Tangent = float4(0, 0, 0, 1);

    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.WorldNormal);
    
    // 머터리얼 샘플링
    float3 DiffuseTex;
    if ((bool) bHasDiffuseMap)
    {
        DiffuseTex = DiffuseMap.Sample(SampleState, input.UV).rgb;
    }
    else
    {
        DiffuseTex = DiffuseColor;
    }
    
    //float3 SpecularTex;
    //if ((bool) bHasSpecularMap)
    //{
    //    SpecularTex = SpecularMap.Sample(SampleState, input.UV).rgb;
    //}
    //else
    //{
    //    SpecularTex = SpecularColor;
    //}
    
    // Blinn-Phong Forward Lighting
    // Ambient
    float3 Finalambient = Ambient.Color * Ambient.Intensity * AmbientColor * DiffuseTex;

    // Directional - Diffuse
    float3 L = normalize(-Directional.Direction);
    float NdotL = max(dot(N, L), 0.0f);
    float FinalDiffuse = Directional.Color * Directional.Intensity * DiffuseTex * NdotL;
    
    //// Directional - Specular (Blinn-Phong)
    //float3 ViewDir = normalize(CameraWorldPos - input.WorldPos);
    //float3 HalfVector = normalize(L + ViewDir);
    //float NdotH = saturate(dot(N, HalfVector));
    //float3 FinalSpecular = Directional.Color * Directional.Intensity * SpecularTex * pow(NdotH, max(Shininess, 1.0f));
    
    //float3 finalColor = (Finalambient + FinalDiffuse + FinalSpecular);
    float3 finalColor = (Finalambient + FinalDiffuse);
    return float4(finalColor, 1.0f);
}