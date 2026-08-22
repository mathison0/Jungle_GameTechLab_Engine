Texture2D backgroundTexture : register(t0);
SamplerState backgroundSampler : register(s0);

cbuffer Constants : register(b0)
{
    float3 Offset;
    float Angle;
    
    float3 Scale;
    int Flag;
    
    float4 Color;
    
    float2 uvOffset;
    float2 uvScale;
    
    float spinAngle;
    float3 pad_constants;
};

cbuffer ConstantPerFrame : register(b1)
{
    float cameraY;
    float3 padding; 
};

struct VS_INPUT
{
    float4 position : POSITION; // Input position from vertex buffer
    float4 color : COLOR; // Input color from vertex buffer
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 color : COLOR; // Color to pass to the pixel shader
    float2 uv : TEXCOORD;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = float4(input.position.xyz * 2, 1.0f);
    
    output.color = input.color;
    output.uv = input.uv;
    
    return output;
}



float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float2 scrolledUV = input.uv;

    scrolledUV.y -= cameraY * 0.1f;
    
    return backgroundTexture.Sample(backgroundSampler, scrolledUV);
}
