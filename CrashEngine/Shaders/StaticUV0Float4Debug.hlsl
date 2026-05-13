/*
    Static UV0 widening validation shader.
    It requests TEXCOORD0 as float4 so the runtime packer must widen UV0(float2)
    into xy + zero-filled zw at upload time.
*/

#include "Passes/Scene/Deferred/DeferredOpaquePass.hlsl"

struct FStaticUV0Float4DebugVSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float4 texcoord : TEXCOORD0;
    float4 tangent  : TANGENT;
};

struct FStaticUV0Float4DebugVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD1;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD2;
    float4 gouraud      : TEXCOORD3;
    float4 texcoord4    : TEXCOORD4;
};

FStaticUV0Float4DebugVSOutput VS(FStaticUV0Float4DebugVSInput Input)
{
    VS_Input_PNCT_T BaseInput;
    BaseInput.position = Input.position;
    BaseInput.normal = Input.normal;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord.xy;
    BaseInput.tangent = Input.tangent;

    FDeferred_Opaque_VSOutput BaseOutput = VS_DeferredOpaque(BaseInput);

    FStaticUV0Float4DebugVSOutput Output;
    Output.position = BaseOutput.position;
    Output.worldNormal = BaseOutput.worldNormal;
    Output.worldTangent = BaseOutput.worldTangent;
    Output.color = BaseOutput.color;
    Output.texcoord = BaseOutput.texcoord;
    Output.gouraud = BaseOutput.gouraud;
    Output.texcoord4 = Input.texcoord;
    return Output;
}

FGBufferOutput3 PS(FStaticUV0Float4DebugVSOutput Input)
{
    FDeferred_Opaque_VSOutput BaseInput;
    BaseInput.position = Input.position;
    BaseInput.worldNormal = Input.worldNormal;
    BaseInput.worldTangent = Input.worldTangent;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord;
    BaseInput.gouraud = Input.gouraud;

    FGBufferOutput3 Output = PS_Opaque_BlinnPhong(BaseInput);

    const float4 UV = Input.texcoord4;
    const float2 GridUV = frac(UV.xy * 8.0f);
    const float Checker = step(1.0f, GridUV.x + GridUV.y);
    const float3 XYColor = float3(frac(UV.x), frac(UV.y), Checker);

    const float UnexpectedZW = saturate(max(abs(UV.z), abs(UV.w)) * 1024.0f);
    const float3 FailureColor = float3(1.0f, 0.0f, 1.0f);

    Output.BaseColor.rgb = lerp(XYColor, FailureColor, UnexpectedZW);
    Output.Surface2.a = 1.0f;
    return Output;
}
