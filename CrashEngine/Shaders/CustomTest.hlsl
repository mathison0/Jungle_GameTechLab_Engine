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

FDeferred_Opaque_VSOutput VS(VS_Input_PNCT_T Input)
{
    return VS_DeferredOpaque(Input);
}

FGBufferOutput3 PS(FDeferred_Opaque_VSOutput Input)
{
    FGBufferOutput3 Output = PS_Opaque_BlinnPhong(Input);

    // Keep the reflected b3 layout alive without changing the visible output.
    const float Epsilon = 1.0e-8f;
    Output.BaseColor.rgb += CustomTint.rgb * saturate(CustomWeights.x) * Epsilon;
    Output.Surface2.a += CustomWeights.y * Epsilon;
    return Output;
}
