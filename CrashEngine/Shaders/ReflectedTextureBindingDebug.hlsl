/*
    Reflection texture binding generalization debug shader.
    It samples a texture bound through shader reflection only, using a non-default
    slot name so file-name-based test-pair exceptions are no longer required.
*/

#include "Passes/Scene/Deferred/DeferredOpaquePass.hlsl"

Texture2D BaseColorTexture : register(t5);

struct FReflectedTextureBindingDebugVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD1;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD2;
    float4 gouraud      : TEXCOORD3;
};

FReflectedTextureBindingDebugVSOutput VS(VS_Input_PNCT_T Input)
{
    FDeferred_Opaque_VSOutput BaseOutput = VS_DeferredOpaque(Input);

    FReflectedTextureBindingDebugVSOutput Output;
    Output.position = BaseOutput.position;
    Output.worldNormal = BaseOutput.worldNormal;
    Output.worldTangent = BaseOutput.worldTangent;
    Output.color = BaseOutput.color;
    Output.texcoord = BaseOutput.texcoord;
    Output.gouraud = BaseOutput.gouraud;
    return Output;
}

FGBufferOutput3 PS(FReflectedTextureBindingDebugVSOutput Input)
{
    FDeferred_Opaque_VSOutput BaseInput;
    BaseInput.position = Input.position;
    BaseInput.worldNormal = Input.worldNormal;
    BaseInput.worldTangent = Input.worldTangent;
    BaseInput.color = Input.color;
    BaseInput.texcoord = Input.texcoord;
    BaseInput.gouraud = Input.gouraud;

    FGBufferOutput3 Output = PS_Opaque_BlinnPhong(BaseInput);

    uint TextureWidth = 0;
    uint TextureHeight = 0;
    BaseColorTexture.GetDimensions(TextureWidth, TextureHeight);

    if (TextureWidth == 0 || TextureHeight == 0)
    {
        Output.BaseColor.rgb = float3(1.0f, 0.0f, 1.0f);
        Output.Surface2.a = 1.0f;
        return Output;
    }

    const float3 ReflectedColor = BaseColorTexture.Sample(LinearWrapSampler, Input.texcoord).rgb;
    const float2 GridUV = frac(Input.texcoord * 8.0f);
    const float Checker = step(1.0f, GridUV.x + GridUV.y);
    const float3 GridColor = float3(GridUV, Checker);

    Output.BaseColor.rgb = lerp(ReflectedColor, GridColor, 0.25f);
    Output.Surface2.a = 1.0f;
    return Output;
}
