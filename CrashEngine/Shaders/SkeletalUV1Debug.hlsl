/*
    Skeletal UV1 validation shader.
    It visualizes TEXCOORD1 directly so multi-UV routing can be confirmed
    without depending on any mesh-specific texture content.
*/

#include "Passes/Scene/Deferred/DeferredOpaquePass.hlsl"

struct FSkeletalUV1DebugVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD1;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD2;
    float4 gouraud      : TEXCOORD3;
    float2 texcoord1    : TEXCOORD4;
};

FSkeletalUV1DebugVSOutput VS(VS_Input_PNCT_T_UV1 Input)
{
    VS_Input_PNCT_T BaseInput;
    BaseInput.position = Input.position;
    BaseInput.normal = Input.normal;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord;
    BaseInput.tangent = Input.tangent;

    FDeferred_Opaque_VSOutput BaseOutput = VS_DeferredOpaque(BaseInput);

    FSkeletalUV1DebugVSOutput Output;
    Output.position = BaseOutput.position;
    Output.worldNormal = BaseOutput.worldNormal;
    Output.worldTangent = BaseOutput.worldTangent;
    Output.color = BaseOutput.color;
    Output.texcoord = BaseOutput.texcoord;
    Output.gouraud = BaseOutput.gouraud;
    Output.texcoord1 = Input.texcoord1;
    return Output;
}

FGBufferOutput3 PS(FSkeletalUV1DebugVSOutput Input)
{
    FDeferred_Opaque_VSOutput BaseInput;
    BaseInput.position = Input.position;
    BaseInput.worldNormal = Input.worldNormal;
    BaseInput.worldTangent = Input.worldTangent;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord;
    BaseInput.gouraud = Input.gouraud;

    FGBufferOutput3 Output = PS_Opaque_BlinnPhong(BaseInput);

    const float2 UV1 = Input.texcoord1;
    const float2 GridUV = frac(UV1 * 8.0f);
    const float Checker = step(1.0f, GridUV.x + GridUV.y);
    const float3 UV1Color = float3(frac(UV1.x), frac(UV1.y), Checker);

    Output.BaseColor.rgb = UV1Color;
    Output.Surface2.a = 1.0f;
    return Output;
}
