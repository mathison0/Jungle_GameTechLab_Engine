/*
    DeferredLightingPS.hlsl는 deferred lighting pass의 진입점입니다.

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
    - LIGHTING_MODEL_WORLDNORMAL
    - LIGHTING_MODEL_UNLIT
    - ENABLE_LIGHT_EVAL_COUNTER
*/

#ifndef ENABLE_LIGHT_EVAL_COUNTER
#define ENABLE_LIGHT_EVAL_COUNTER 0
#endif

Texture2D g_BaseColorTex : register(t0);
Texture2D g_Surface1Tex : register(t1);
Texture2D g_Surface2Tex : register(t2);

Texture2D g_ModifiedBaseColorTex : register(t3);
Texture2D g_ModifiedSurface1Tex : register(t4);
Texture2D g_ModifiedSurface2Tex : register(t5);

#if ENABLE_LIGHT_EVAL_COUNTER
RWBuffer<uint> GlobalLightEvalCounter : register(u1);
#endif

#define USE_LIGHT_CULLING 1
#include "../../../Render/Scene/Shared/ViewModeCommon.hlsli"

float4 ResolveBaseColor(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_BaseColorTex, g_ModifiedBaseColorTex, UV);
}

float4 ResolveSurface1(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_Surface1Tex, g_ModifiedSurface1Tex, UV);
}

float4 ResolveSurface2(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_Surface2Tex, g_ModifiedSurface2Tex, UV);
}

PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS_UberLit(PS_Input_UV Input) : SV_TARGET0
{
    float2 UV = Input.uv;
    float SceneDepthValue = SceneDepth.Sample(PointClampSampler, UV).r;
    if (SceneDepthValue <= 0.0f)
    {
        discard;
    }

    float4 BaseColor = ResolveBaseColor(UV);
    float4 FinalColor = BaseColor;

#if defined(LIGHTING_MODEL_GOURAUD)
    float4 GouraudLighting = ResolveSurface1(UV);
    FinalColor = ComputeGouraudLighting(BaseColor, GouraudLighting);

#elif defined(LIGHTING_MODEL_LAMBERT)
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    FinalColor = ComputeDeferredLambertLighting(BaseColor, Normal, WorldPos, Input.position);

#elif defined(LIGHTING_MODEL_BLINNPHONG)
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDir = normalize(CameraWorldPos - WorldPos);
    FinalColor = ComputeDeferredBlinnPhongLighting(BaseColor, Normal, MaterialParam, WorldPos, ViewDir, Input.position);

#elif defined(LIGHTING_MODEL_WORLDNORMAL)
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    FinalColor = float4(Normal * 0.5f + 0.5f, 1.0f);

#elif defined(LIGHTING_MODEL_UNLIT)
    FinalColor = BaseColor;

#else
    FinalColor = BaseColor;
#endif

    return FinalColor;
}
