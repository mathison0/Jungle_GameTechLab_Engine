#include "Common.hlsl"

#define UBERLIT_DEBUG_SPEC_MODE 0
// 0: off, 1: NdotL, 2: NdotH, 3: spec

// VIEW_MODE: 0(Lit), 5(Gouraud), 6(Lambert), 7(Phong)
// USE_NORMALMAP: 0 or 1
#ifndef VIEW_MODE
    #define VIEW_MODE 7  // 기본값 설정
#endif

// StaticMesh Material (b6)
cbuffer StaticMeshBuffer : register(b6)
{
    float3 AmbientColor; // Ka
    float3 DiffuseColor; // Kd
    float3 SpecularColor; // Ks
    float Shininess; // Ns
    // Camera
    float3 CameraWorldPos;
    // ScrollUV
    float2 ScrollUV;
    uint bHasDiffuseMap;
    uint bHasSpecularMap;
    
    uint bHasNormalMap;
    float Padding6_1;
    float Padding6_2;
    float Padding6_3;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldTangent : TEXCOORD2;
    float2 UV : TEXCOORD3;
    float3 VertexDiffuseLighting : TEXCOORD4;
    float3 VertexSpecularLighting : TEXCOORD5;
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

struct FPointLightCommon
{
    float3 LightDir;
    float Distance;
    float Attenuation;
    float NdotL;
    bool bValid;
};

struct FSpotLightInfo
{
    float3 Position;
    float Radius;

    float3 Color;
    float Intensity;
    
    float3 Direction;
    float InnerConeCos;
    
    float OuterConeCos;
    float3 Padding;
};

cbuffer Lighting : register(b13)
{
    FAmbientLightInfo Ambient;
    uint DirectionalLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    float LightingPad;
};

// Light Data (t0-t5)
StructuredBuffer<FPointLightInfo> PointLights : register(t0);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t1);
StructuredBuffer<uint> TilePointLightIndices : register(t2);
StructuredBuffer<uint> TileSpotLightIndices : register(t3);
StructuredBuffer<uint2> TilePointLightGrid : register(t4);
StructuredBuffer<uint2> TileSpotLightGrid : register(t5);

// Directional Lights (t10)

StructuredBuffer<FDirectionalLightInfo> DirectionalLights : register(t10);

// StaticMesh Textures (t6-t9)
Texture2D DiffuseMap : register(t6);
Texture2D AmbientMap : register(t7);
Texture2D SpecularMap : register(t8);
Texture2D BumpMap : register(t9);

SamplerState SampleState : register(s0);

float3 GetDiffuseTexPS(float2 uv)
{
    if ((bool) bHasDiffuseMap)
    {
        return DiffuseMap.Sample(SampleState, uv).rgb;
    }
    return DiffuseColor;
}

float3 GetSpecularTexPS(float2 uv)
{
    if ((bool) bHasSpecularMap)
    {
        return SpecularMap.Sample(SampleState, uv).rgb;
    }
    return SpecularColor;
}

float3 GetDiffuseTexVS(float2 uv)
{
    if ((bool) bHasDiffuseMap)
    {
        return DiffuseMap.SampleLevel(SampleState, uv, 0).rgb;
    }
    return DiffuseColor;
}

float3 GetSpecularTexVS(float2 uv)
{
    if ((bool) bHasSpecularMap)
    {
        return SpecularMap.SampleLevel(SampleState, uv, 0).rgb;
    }
    return SpecularColor;
}

float3 CalculateAmbientLight(FAmbientLightInfo Light, float3 MaterialAmbientColor, float3 DiffuseTex)
{
    return Light.Color * Light.Intensity * MaterialAmbientColor * DiffuseTex;
}

FPointLightCommon EvaluatePointLightCommon(
    FPointLightInfo Light,
    float3 WorldPos,
    float3 N)
{
    FPointLightCommon Result;
    Result.bValid = false;
    Result.LightDir = 0.0f.xxx;
    Result.Distance = 0.0f;
    Result.Attenuation = 0.0f;
    Result.NdotL = 0.0f;

    float3 L = Light.Position - WorldPos;
    float distSq = dot(L, L);
    float radiusSq = Light.Radius * Light.Radius;

    if (distSq > radiusSq)
        return Result;

    float dist = sqrt(distSq);
    float3 lightDir = L / max(dist, 1e-5f);

    N = normalize(N);
    float NdotL = saturate(dot(N, lightDir));
    if (NdotL <= 0.0f)
        return Result;

    float attenuation = saturate(1.0f - dist / Light.Radius);
    attenuation *= attenuation;

    Result.bValid = true;
    Result.LightDir = lightDir;
    Result.Distance = dist;
    Result.Attenuation = attenuation;
    Result.NdotL = NdotL;
    return Result;
}

float3 CalculatePointDiffuse(
    FPointLightInfo Light,
    float3 DiffuseTex,
    FPointLightCommon Common)
{
    if (!Common.bValid)
        return 0.0f.xxx;

    return DiffuseTex * Light.Color.xyz * Light.Intensity * Common.NdotL * Common.Attenuation;
}

float3 CalculatePointSpecular(
    FPointLightInfo Light,
    float3 N,
    float3 WorldPos,
    float3 CameraWorldPos,
    float3 SpecularTex,
    float Shininess,
    FPointLightCommon Common)
{
    if (!Common.bValid)
        return 0.0f.xxx;

    float3 V = normalize(CameraWorldPos - WorldPos);
    float3 H = normalize(Common.LightDir + V);
    float NdotH = saturate(dot(normalize(N), H));
    float spec = pow(NdotH, Shininess);

    return Light.Color.xyz * Light.Intensity * SpecularTex * spec * Common.Attenuation;
}

float3 CalculateDirectionalDiffuse(FDirectionalLightInfo Light, float3 N, float3 DiffuseTex)
{
    float3 L = normalize(-Light.Direction);
    float NdotL = max(dot(N, L), 0.0f);
    return Light.Color * Light.Intensity * DiffuseTex * NdotL;
}

float3 CalculateDirectionalSpecular(FDirectionalLightInfo Light, float3 N, float3 WorldPos, float3 CameraWorldPos, float3 SpecularTex, float Shininess)
{
    float3 L = normalize(-Light.Direction);
    float3 ViewDir = normalize(CameraWorldPos - WorldPos);
    float3 HalfVector = normalize(L + ViewDir);
    float NdotH = saturate(dot(N, HalfVector));
    return Light.Color * Light.Intensity * SpecularTex * pow(NdotH, max(Shininess, 1.0f));
}

float3 CalculateSpotDiffuse(FSpotLightInfo Light, float3 N, float3 WorldPos, float3 DiffuseTex)
{
    float3 Lvec = Light.Position - WorldPos;
    float dist = length(Lvec);

    if (dist > Light.Radius)
        return 0;

    float3 L = Lvec / dist;

    float NdotL = max(dot(N, L), 0.0f);

    float3 lightDir = normalize(-Light.Direction);

    float spotCos = dot(L, lightDir);
    float spotFactor = smoothstep(
        Light.OuterConeCos,
        Light.InnerConeCos,
        spotCos
    );
    spotFactor = spotFactor * spotFactor;

    float attenuation = 1.0f - (dist / Light.Radius);
    attenuation = attenuation * attenuation;

    return Light.Color *
           Light.Intensity *
           DiffuseTex *
           NdotL *
           attenuation *
           spotFactor;
}

float3 CalculateSpotSpecular(FSpotLightInfo Light, float3 N, float3 WorldPos, float3 CameraWorldPos, float3 SpecularTex, float Shininess)
{
    float3 Lvec = Light.Position - WorldPos;
    float dist = length(Lvec);

    if (dist > Light.Radius)
        return 0;

    float3 L = Lvec / dist;

    float3 V = normalize(CameraWorldPos - WorldPos);
    float3 H = normalize(L + V);

    float NdotH = saturate(dot(N, H));

    float3 lightDir = normalize(-Light.Direction);
    float spotCos = dot(L, lightDir);

    float spotFactor = smoothstep(
        Light.OuterConeCos,
        Light.InnerConeCos,
        spotCos
    );
    spotFactor *= spotFactor;

    float attenuation = 1.0f - (dist / Light.Radius);
    attenuation *= attenuation;

    return Light.Color *
           Light.Intensity *
           SpecularTex *
           pow(NdotH, max(Shininess, 1.0f)) *
           attenuation *
           spotFactor;
}

void CalculateLightingLambert(float3 WorldPos, float3 N, out float3 OutDiffuse)
{
    OutDiffuse = CalculateAmbientLight(Ambient, AmbientColor, 1.0f.xxx);

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        OutDiffuse += DirectionalLights[i].Color * DirectionalLights[i].Intensity * NdotL;
    }

    for (uint j = 0; j < PointLightCount; ++j)
    {
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[j], WorldPos, N);
        if (Common.bValid)
        {
            OutDiffuse += PointLights[j].Color * PointLights[j].Intensity * Common.NdotL * Common.Attenuation;
        }
    }

    for (uint k = 0; k < SpotLightCount; ++k)
    {
        float3 Lvec = SpotLights[k].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[k].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[k].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[k].OuterConeCos, SpotLights[k].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[k].Radius);
        attenuation *= attenuation;

        OutDiffuse += SpotLights[k].Color * SpotLights[k].Intensity * NdotL * attenuation * spotFactor;
    }
}

void CalculateLightingBlinnPhong(
    float3 WorldPos,
    float3 N,
    out float3 OutDiffuse,
    out float3 OutSpecular)
{
    CalculateLightingLambert(WorldPos, N, OutDiffuse);
    OutSpecular = 0.0f.xxx;

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += DirectionalLights[i].Color *
                       DirectionalLights[i].Intensity *
                       pow(NdotH, max(Shininess, 1.0f));
    }

    for (uint j = 0; j < PointLightCount; ++j)
    {
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[j], WorldPos, N);
        if (!Common.bValid)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(Common.LightDir + V);
        float NdotH = saturate(dot(normalize(N), H));

        OutSpecular += PointLights[j].Color *
                       PointLights[j].Intensity *
                       pow(NdotH, max(Shininess, 1.0f)) *
                       Common.Attenuation;
    }

    for (uint k = 0; k < SpotLightCount; ++k)
    {
        float3 Lvec = SpotLights[k].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[k].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[k].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[k].OuterConeCos, SpotLights[k].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[k].Radius);
        attenuation *= attenuation;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += SpotLights[k].Color *
                       SpotLights[k].Intensity *
                       pow(NdotH, max(Shininess, 1.0f)) *
                       attenuation *
                       spotFactor;
    }
}

PSInput VS(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    output.WorldPos = worldPos.xyz;
    output.ClipPos = mul(mul(worldPos, View), Projection);
    
    // 비균일 스케일을 위한 역행렬 이후 전치 
    // 역행렬은 비용이 많이 들어서 상수 버퍼로 가져오는 게 나을 거 같네요...
    float3x3 normalMatrix = transpose(Inverse3x3((float3x3) Model));
    output.WorldNormal = normalize(mul(input.Normal, normalMatrix));
    output.WorldTangent = normalize(mul(input.Tangent, normalMatrix));

    output.UV = input.UV + ScrollUV;

    
    // Gouraud Lighting
    {
        float3 N = normalize(output.WorldNormal);

        CalculateLightingBlinnPhong(
            output.WorldPos,
            N,
            output.VertexDiffuseLighting,
            output.VertexSpecularLighting);
        // 블린퐁
        // output.VertexLighting = CalculateLightingBlinnPhong(output.WorldPos, N, diffuseTex, GetSpecularTex(output.UV));

    }

    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.WorldNormal);
    
    float3 DiffuseTex = GetDiffuseTexPS(input.UV);
    float3 SpecularTex = GetSpecularTexPS(input.UV);
    
    return float4(input.WorldTangent, 1.0f);
   
    float3 finalColor = 0;
 
 #if VIEW_MODE == 5  //Gouraud
    finalColor =
        DiffuseTex * input.VertexDiffuseLighting +
        SpecularTex * input.VertexSpecularLighting;

#elif VIEW_MODE == 6 //Lambert
     float3 DiffuseLighting;
     CalculateLightingLambert(input.WorldPos, N, DiffuseLighting);
     finalColor = DiffuseTex * DiffuseLighting;

#elif VIEW_MODE == 7 //BlinnPhong
     float3 DiffuseLighting;    
     float3 SpecularLighting;
     CalculateLightingBlinnPhong(input.WorldPos, N, DiffuseLighting, SpecularLighting);
     finalColor = DiffuseTex * DiffuseLighting + SpecularTex * SpecularLighting;
#endif
    return float4(finalColor, 1.0f);
}
