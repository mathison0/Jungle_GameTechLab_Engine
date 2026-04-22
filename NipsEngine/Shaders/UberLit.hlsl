#include "Common.hlsl"

#define UBERLIT_DEBUG_SPEC_MODE 0
// 0: off, 1: NdotL, 2: NdotH, 3: spec

#define LIT 0
#define UNLIT 1
#define LIT_GOURAUD 5
#define LIT_LAMBERT 6
#define LIT_PHONG 7
#define WORLD_NORMAL 8
#define TILE_LIGHT_HITMAP 9

// USE_NORMALMAP: 0 or 1
#define TRUE 1
#define FALSE 0

// OPAQUE TYPE
#define STATICMESH 0
#define DECAL 1

#ifndef VIEW_MODE
    #define VIEW_MODE 7  // 기본값 설정
#endif

#if OPAQUETYPE == DECAL
Texture2D g_NormalTexture : register(t11);
Texture2D g_DepthTexture : register(t13);
#endif

struct PSOutput
{
    float4 Color : SV_Target0;
    
#if OPAQUETYPE == STATICMESH
    float4 Normal : SV_Target1;
#endif
};

#ifndef FORWARD_PLUS_TILE_SIZE_X
    #define FORWARD_PLUS_TILE_SIZE_X 16
#endif

#ifndef FORWARD_PLUS_TILE_SIZE_Y
    #define FORWARD_PLUS_TILE_SIZE_Y 16
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

cbuffer DecalBuffer : register(b7)
{
    row_major float4x4 InverseClipToLocal;
    float FadeAlpha;
    float3 DecalAmbientColor;
    
    float3 DecalDiffuseColor;
    uint bHasDecalDiffuseMap;
    
    float3 DecalSpecularColor;
    uint bHasDecalSpecularMap;
    
    uint bHasDecalNormalMap;
    float3 Padding7;
}

cbuffer SceneDepthBuffer : register(b10)
{
    float2 ViewportUVOffset;
    float2 ViewportUVScale;
    float2 SceneDepthTextureSize;
    float2 Pad;
}

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
    
    float4 ScreenPos : TEXCOORD6;
    
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

cbuffer ForwardPlusConstants : register(b11)
{
    uint2 ViewportMin;
    uint2 ViewportSize;
    uint2 DepthTextureSize;
    uint2 TileCount;
    uint bEnable25DMask;
    float3 ForwardPlusPadding;
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

// StaticMesh/Decal Textures (t6-t9)
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

uint GetTileIndexFromScreenPos(float2 screenPos)
{
    uint tileCountX = max(TileCount.x, 1u);
    uint tileCountY = max(TileCount.y, 1u);

    float2 localPixel = max(screenPos - float2(ViewportMin), 0.0f.xx);
    uint2 tileCoord = uint2(localPixel / float2(FORWARD_PLUS_TILE_SIZE_X, FORWARD_PLUS_TILE_SIZE_Y));
    tileCoord.x = min(tileCoord.x, tileCountX - 1u);
    tileCoord.y = min(tileCoord.y, tileCountY - 1u);

    return tileCoord.y * tileCountX + tileCoord.x;
}

float3 EvaluateHitmapRamp(float t)
{
    t = saturate(t);

    const float3 c0 = float3(0.02f, 0.02f, 0.04f);
    const float3 c1 = float3(0.06f, 0.22f, 0.68f);
    const float3 c2 = float3(0.00f, 0.72f, 0.84f);
    const float3 c3 = float3(0.96f, 0.86f, 0.18f);
    const float3 c4 = float3(0.92f, 0.20f, 0.10f);

    if (t < 0.25f)
    {
        return lerp(c0, c1, t / 0.25f);
    }

    if (t < 0.50f)
    {
        return lerp(c1, c2, (t - 0.25f) / 0.25f);
    }

    if (t < 0.75f)
    {
        return lerp(c2, c3, (t - 0.50f) / 0.25f);
    }

    return lerp(c3, c4, (t - 0.75f) / 0.25f);
}

float3 CalculateTileLightHitmap(uint TileIndex, float2 screenPos)
{
    uint pointCount = TilePointLightGrid[TileIndex].y;
    uint spotCount = TileSpotLightGrid[TileIndex].y;
    uint totalCount = pointCount + spotCount;

    // A log scale makes low-to-mid tile densities easier to inspect.
    float normalizedCount = saturate(log2((float)totalCount + 1.0f) / log2(33.0f));
    float3 baseColor = EvaluateHitmapRamp(normalizedCount);

    float pointWeight = (totalCount > 0u) ? ((float)pointCount / (float)totalCount) : 0.0f;
    float spotWeight = (totalCount > 0u) ? ((float)spotCount / (float)totalCount) : 0.0f;
    baseColor += float3(0.18f * pointWeight, 0.00f, 0.18f * spotWeight);

    float2 localPixel = max(screenPos - float2(ViewportMin), 0.0f.xx);
    float2 tileCoordPixel = frac(localPixel / float2(FORWARD_PLUS_TILE_SIZE_X, FORWARD_PLUS_TILE_SIZE_Y));
    float2 tileEdgeDistance =
        min(tileCoordPixel, 1.0f - tileCoordPixel) * float2(FORWARD_PLUS_TILE_SIZE_X, FORWARD_PLUS_TILE_SIZE_Y);
    float edgeDistance = min(tileEdgeDistance.x, tileEdgeDistance.y);
    float gridLine = 1.0f - smoothstep(0.3f, 0.6f, edgeDistance);

    return lerp(baseColor, 1.0f.xxx, gridLine * 0.3f);
}

void CalculateLightingLambertTile(float3 WorldPos, float3 N, uint TileIndex, out float3 OutDiffuse)
{
    OutDiffuse = CalculateAmbientLight(Ambient, AmbientColor, 1.0f.xxx);

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        OutDiffuse += DirectionalLights[i].Color * DirectionalLights[i].Intensity * NdotL;
    }

    uint2 pointGrid = TilePointLightGrid[TileIndex];
    for (uint i = 0; i < pointGrid.y; ++i)
    {
        uint lightIndex = TilePointLightIndices[pointGrid.x + i];
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[lightIndex], WorldPos, N);
        if (Common.bValid)
        {
            OutDiffuse += PointLights[lightIndex].Color *
                          PointLights[lightIndex].Intensity *
                          Common.NdotL *
                          Common.Attenuation;
        }
    }

    uint2 spotGrid = TileSpotLightGrid[TileIndex];
    for (uint i = 0; i < spotGrid.y; ++i)
    {
        uint lightIndex = TileSpotLightIndices[spotGrid.x + i];
        float3 Lvec = SpotLights[lightIndex].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[lightIndex].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[lightIndex].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[lightIndex].OuterConeCos, SpotLights[lightIndex].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[lightIndex].Radius);
        attenuation *= attenuation;

        OutDiffuse += SpotLights[lightIndex].Color *
                      SpotLights[lightIndex].Intensity *
                      NdotL *
                      attenuation *
                      spotFactor;
    }
}

void CalculateLightingBlinnPhongTile(
    float3 WorldPos,
    float3 N,
    uint TileIndex,
    out float3 OutDiffuse,
    out float3 OutSpecular)
{
    CalculateLightingLambertTile(WorldPos, N, TileIndex, OutDiffuse);
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

    uint2 pointGrid = TilePointLightGrid[TileIndex];
    for (uint i = 0; i < pointGrid.y; ++i)
    {
        uint lightIndex = TilePointLightIndices[pointGrid.x + i];
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[lightIndex], WorldPos, N);
        if (!Common.bValid)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(Common.LightDir + V);
        float NdotH = saturate(dot(normalize(N), H));

        OutSpecular += PointLights[lightIndex].Color *
                       PointLights[lightIndex].Intensity *
                       pow(NdotH, max(Shininess, 1.0f)) *
                       Common.Attenuation;
    }

    uint2 spotGrid = TileSpotLightGrid[TileIndex];
    for (uint i = 0; i < spotGrid.y; ++i)
    {
        uint lightIndex = TileSpotLightIndices[spotGrid.x + i];
        float3 Lvec = SpotLights[lightIndex].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[lightIndex].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[lightIndex].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[lightIndex].OuterConeCos, SpotLights[lightIndex].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[lightIndex].Radius);
        attenuation *= attenuation;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += SpotLights[lightIndex].Color *
                       SpotLights[lightIndex].Intensity *
                       pow(NdotH, max(Shininess, 1.0f)) *
                       attenuation *
                       spotFactor;
    }
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
    output.ScreenPos = output.ClipPos;
    
    float3x3 normalMatrix = transpose(InvModel);
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

    }

    return output;
}

PSOutput PS(PSInput input)
{
    PSOutput Output;
    float4 finalColor = { 0.f, 0.f, 0.f, 1.0f };
    float3 N = normalize(input.WorldNormal);
    uint TileIndex = GetTileIndexFromScreenPos(input.ClipPos.xy);
    
    float3 DiffuseTex = { 1.0f, 1.0f, 1.0f };
    float3 SpecularTex = { 1.0f, 1.0f, 1.0f };
    
    float3 DiffuseLighting;
    
    //Decal PixelShader Logic
    #if OPAQUETYPE == DECAL
    float2 ndcXY = input.ScreenPos.xy / input.ScreenPos.w;
    
    float2 screenUV = ndcXY * float2(0.5f, -0.5f) + 0.5f;
    float2 DepthUV = ViewportUVOffset + ViewportUVScale * screenUV;
    
    float depthZ = g_DepthTexture.Sample(SampleState, DepthUV).r;
    
    if (depthZ >= 1.0f) 
        discard;
    
    float4 clipPos = float4(ndcXY, depthZ, 1.0f);
    float4 localPos = mul(clipPos, InverseClipToLocal);
    localPos /= localPos.w;
    
    if (any(abs(localPos.xyz) > 0.501f)) 
        clip(-1);
    
    float2 decalUV = float2(localPos.y, -localPos.z) + 0.5f;
    finalColor = DiffuseMap.Sample(SampleState, decalUV);
    finalColor.a *= FadeAlpha;
  
    if (finalColor.a < 0.05f) 
        clip(-1);
  
    //Normal Sampling
    
    float3 sceneNormal = g_NormalTexture.Sample(SampleState, DepthUV).rgb;
    sceneNormal = normalize(sceneNormal * 2.0f -1.0f);
    float3 finalNormal = sceneNormal;
    
    #if USE_NORMALMAP == TRUE
    float3 decalNormalTex = BumpMap.Sample(SampleState, decalUV).rgb * 2.0f - 1.0f;
    finalNormal = normalize(sceneNormal + decalNormalTex);
    #endif
    
    //월드 좌표 복원
    float4 worldPosDecal = mul(clipPos, InverseViewProjection);
    worldPosDecal /= worldPosDecal.w;
    
    //조명 계산
    float3 dDiffuse = 0;
    float3 dSpecular = 0;
    
    DiffuseTex = DiffuseMap.Sample(SampleState, decalUV).rgb;
    SpecularTex = SpecularMap.Sample(SampleState, decalUV).rgb;

    
#if VIEW_MODE == UNLIT 
    if (!(bool)bHasDecalDiffuseMap)
    {
        DiffuseTex = float3(1.f, 1.f, 1.f);
        finalColor.xyz = DiffuseColor;
    }
    else
    {
        finalColor.xyz = DiffuseTex;
    }
    
    #elif VIEW_MODE == LIT_LAMBERT
     CalculateLightingLambert(input.WorldPos, finalNormal, DiffuseLighting);
     finalColor.xyz = DiffuseTex * DiffuseLighting;
    
    #elif VIEW_MODE == LIT_PHONG
    CalculateLightingBlinnPhong(worldPosDecal.xyz, finalNormal, dDiffuse, dSpecular);
    finalColor.xyz = (finalColor.rgb * dDiffuse) + (SpecularTex * dSpecular);
    
    #elif VIEW_MODE == TILE_LIGHT_HITMAP
    finalColor.xyz = CalculateTileLightHitmap(TileIndex, input.ClipPos.xy);

    #elif VIEW_MODE == WORLD_NORMAL
    discard;

    #endif

    if (finalColor.a < 0.05f)
        discard;
    
    Output.Color = finalColor;
    return Output;
    
    #elif OPAQUETYPE == STATICMESH // StaticMesh PixelShader Logic
    DiffuseTex = DiffuseColor;
    
    #if USE_NORMALMAP == TRUE
    float3 T = normalize(input.WorldTangent);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float3 NormalTex = BumpMap.Sample(SampleState, input.UV).rgb * 2.0f - 1.0f;
    N = normalize(mul(NormalTex,TBN));
    #endif
    
    SpecularTex = GetSpecularTexPS(input.UV);
    if ((bool)bHasDiffuseMap)
    {
        DiffuseTex = GetDiffuseTexPS(input.UV);
    }
    

 #if VIEW_MODE == UNLIT
    finalColor.xyz = DiffuseTex;
 
 #elif VIEW_MODE == LIT_GOURAUD
    finalColor.xyz =
        DiffuseTex * input.VertexDiffuseLighting +
        SpecularTex * input.VertexSpecularLighting;

#elif VIEW_MODE == LIT_LAMBERT
     CalculateLightingLambertTile(input.WorldPos, N, TileIndex, DiffuseLighting);
     finalColor.xyz = DiffuseTex * DiffuseLighting;
    
     //ShaderHotReload 테스트용 코드 
     //finalColor = DiffuseTex * float3(1.0f, 1.0f, 1.0f);
    

#elif VIEW_MODE == LIT_PHONG  
     float3 SpecularLighting;
     CalculateLightingBlinnPhongTile(input.WorldPos, N, TileIndex, DiffuseLighting, SpecularLighting);
     
    finalColor.xyz = DiffuseTex * DiffuseLighting + SpecularTex * SpecularLighting;
#elif VIEW_MODE == WORLD_NORMAL
    finalColor.xyz = N * 0.5f + 0.5f;

#elif VIEW_MODE == TILE_LIGHT_HITMAP //TileLightHitmap
    finalColor.xyz = CalculateTileLightHitmap(TileIndex, input.ClipPos.xy);
    
#endif
    Output.Color =  finalColor;
#endif
    
    #if OPAQUETYPE == STATICMESH
    Output.Normal = float4(N, 1.0f);
    #endif
    
    return Output;
}
