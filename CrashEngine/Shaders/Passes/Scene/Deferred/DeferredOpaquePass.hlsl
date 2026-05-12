/*
    DeferredOpaquePass.hlsl는 deferred opaque pass의 진입점입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 텍스처/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

/*
    Available preprocessor defines:
    - LIGHTING_MODEL_GOURAUD
    - LIGHTING_MODEL_LAMBERT
    - LIGHTING_MODEL_BLINNPHONG
    - LIGHTING_MODEL_UNLIT
    - OUTPUT_GOURAUD_L
    - OUTPUT_NORMAL
    - OUTPUT_MATERIAL_PARAM
    - USE_NORMAL_MAP
*/

#include "../../../Utils/Functions.hlsl"
#include "../../../Render/Scene/Shared/OpaquePassTypes.hlsli"
#include "../../../Render/Scene/Material/SurfaceEvaluation.hlsli"
#include "../../../Surface/SurfaceTypes.hlsli"
#include "../../../Surface/GBufferPacking.hlsli"
#include "../../../Render/Scene/Lighting/LightingEvaluation.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#define DEFERRED_NORMAL_TEXTURE g_NormalMap
#else
#define DEFERRED_NORMAL_TEXTURE g_txColor
#endif

Texture2D g_SpecularMap : register(t2);

FSurfaceData BuildDeferredSurfaceData(FDeferred_Opaque_VSOutput Input)
{
    return BuildStaticMeshSurfaceData(Input, g_txColor, DEFERRED_NORMAL_TEXTURE, g_SpecularMap);
}

FDeferred_Opaque_VSOutput VS_DeferredOpaque(VS_Input_PNCT_T Input)
{
    FDeferred_Opaque_VSOutput Output;
    Output.position         = ApplyMVP(Input.position);
    Output.worldNormal      = normalize(mul(Input.normal, (float3x3)NormalMatrix));
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3)NormalMatrix));
    Output.worldTangent.w   = Input.tangent.w;
    Output.color            = Input.color;
    Output.texcoord         = Input.texcoord;

    float3 WorldPos        = mul(float4(Input.position, 1.0f), Model).xyz;
    float3 GouraudLighting = ComputeGouraudLightingColor(Output.worldNormal, WorldPos, Output.position);
    Output.gouraud         = float4(GouraudLighting, 1.0f);
    return Output;
}

float4 PS_Opaque_Unlit(FDeferred_Opaque_VSOutput Input) : SV_TARGET0
{
    FSurfaceData Surface = BuildDeferredSurfaceData(Input);
    return EncodeBaseColor(float4(Surface.BaseColor, Surface.Opacity));
}

FGBufferOutput2 PS_Opaque_Gouraud(FDeferred_Opaque_VSOutput Input)
{
    return EncodeGBuffer_Gouraud(BuildDeferredSurfaceData(Input));
}

FGBufferOutput2 PS_Opaque_Lambert(FDeferred_Opaque_VSOutput Input)
{
    return EncodeGBuffer_Lambert(BuildDeferredSurfaceData(Input));
}

FGBufferOutput3 PS_Opaque_BlinnPhong(FDeferred_Opaque_VSOutput Input)
{
    return EncodeGBuffer_BlinnPhong(BuildDeferredSurfaceData(Input));
}

#undef DEFERRED_NORMAL_TEXTURE
