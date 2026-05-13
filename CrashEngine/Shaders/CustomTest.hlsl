/*
    Custom deferred opaque wrapper for Phase 4 validation.
    It preserves a reflected material cbuffer at b3 while delegating the
    actual surface generation to the engine's built-in deferred opaque path.
*/

#include "Passes/Scene/Deferred/DeferredOpaquePass.hlsl"

cbuffer CustomTestMaterialParams : register(b3)
{
    float4 CustomTint;
    float4 CustomWeights;
}

struct FCustomTestVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD1;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD2;
    float2 texcoord1    : TEXCOORD3;
};

FCustomTestVSOutput VS(VS_Input_PNCT_T_UV1 Input)
{
    VS_Input_PNCT_T BaseInput;
    BaseInput.position = Input.position;
    BaseInput.normal = Input.normal;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord;
    BaseInput.tangent = Input.tangent;

    FDeferred_Opaque_VSOutput BaseOutput = VS_DeferredOpaque(BaseInput);

    FCustomTestVSOutput Output;
    Output.position = BaseOutput.position;
    Output.worldNormal = BaseOutput.worldNormal;
    Output.worldTangent = BaseOutput.worldTangent;
    Output.color = BaseOutput.color;
    Output.texcoord = BaseOutput.texcoord;
    Output.texcoord1 = Input.texcoord1;
    return Output;
}

FGBufferOutput3 PS(FCustomTestVSOutput Input)
{
    FDeferred_Opaque_VSOutput BaseInput;
    BaseInput.position = Input.position;
    BaseInput.worldNormal = Input.worldNormal;
    BaseInput.worldTangent = Input.worldTangent;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord;

    FGBufferOutput3 Output = PS_Opaque_BlinnPhong(BaseInput);

    // Keep the reflected b3 layout alive and TEXCOORD1 wired without changing visible output.
    const float Epsilon = 1.0e-8f;
    Output.BaseColor.rgb += CustomTint.rgb * saturate(CustomWeights.x + Input.texcoord1.x * 0.0f) * Epsilon;
    Output.Surface2.a += CustomWeights.y * Epsilon;
    return Output;
}
