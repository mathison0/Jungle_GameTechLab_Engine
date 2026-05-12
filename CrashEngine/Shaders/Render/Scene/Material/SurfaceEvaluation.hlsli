#ifndef SURFACE_EVALUATION_HLSLI
#define SURFACE_EVALUATION_HLSLI

#include "../../../Surface/SurfaceTypes.hlsli"
#include "../Shared/OpaquePassTypes.hlsli"
#include "MaterialSampling.hlsli"

float3 ResolveStaticMeshSurfaceNormal(float3 WorldNormal, float4 WorldTangent, float2 UV, Texture2D NormalMap)
{
    float3 N = normalize(WorldNormal);

#if defined(USE_NORMAL_MAP)
    if (StaticMeshHasNormalTexture())
    {
        float3 T = normalize(WorldTangent.xyz);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * WorldTangent.w;
        float3x3 TBN = float3x3(T, B, N);
        float3 NormalSample = NormalMap.Sample(LinearWrapSampler, UV).rgb;
        float3 TangentNormal = NormalSample * 2.0f - 1.0f;
        return normalize(mul(TangentNormal, TBN));
    }
#endif

    return N;
}

float4 ResolveStaticMeshMaterialParam(float2 UV, Texture2D SpecularMap)
{
    float Shininess = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;

    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= SpecularMap.Sample(LinearWrapSampler, UV).r;
    }

    return float4(Shininess, SpecularStrength, 0.0f, 1.0f);
}

FSurfaceData BuildStaticMeshSurfaceData(
    float2 UV,
    float3 WorldNormal,
    float4 WorldTangent,
    float4 Gouraud,
    Texture2D BaseColorTexture,
    Texture2D NormalMap,
    Texture2D SpecularMap)
{
    FSurfaceData Surface = (FSurfaceData)0;
    float4 BaseColor = SampleStaticMeshBaseColor(BaseColorTexture, UV) * GetStaticMeshSectionColorOrWhite();
    float4 MaterialInfo = ResolveStaticMeshMaterialParam(UV, SpecularMap);

    Surface.BaseColor = BaseColor.rgb;
    Surface.Opacity = BaseColor.a;
    Surface.WorldNormal = ResolveStaticMeshSurfaceNormal(WorldNormal, WorldTangent, UV, NormalMap);
    Surface.Roughness = MaterialInfo.x;
    Surface.Specular = MaterialInfo.y;
    Surface.Metallic = 0.0f;
    Surface.AmbientOcclusion = 1.0f;
    Surface.Gouraud = Gouraud;
    return Surface;
}

FSurfaceData BuildStaticMeshSurfaceData(
    FForward_Opaque_VSOutput Input,
    Texture2D BaseColorTexture,
    Texture2D NormalMap,
    Texture2D SpecularMap)
{
    return BuildStaticMeshSurfaceData(
        Input.texcoord,
        Input.worldNormal,
        Input.worldTangent,
        Input.gouraud,
        BaseColorTexture,
        NormalMap,
        SpecularMap);
}

FSurfaceData BuildStaticMeshSurfaceData(
    FDeferred_Opaque_VSOutput Input,
    Texture2D BaseColorTexture,
    Texture2D NormalMap,
    Texture2D SpecularMap)
{
    return BuildStaticMeshSurfaceData(
        Input.texcoord,
        Input.worldNormal,
        Input.worldTangent,
        Input.gouraud,
        BaseColorTexture,
        NormalMap,
        SpecularMap);
}

#endif
