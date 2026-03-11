Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

cbuffer Constants : register(b0)
{
    float3 Offset;     // 12바이트
    float Angle;       // 4바이트 (여기까지 16바이트, 1번 레지스터 꽉 참)
    
    float3 Scale;      // 12바이트
    int Flag;          // 4바이트 (기존 isUI/uvOffset 대체. 여기까지 16바이트, 2번 레지스터 꽉 참)
    
    float4 Color;      // 16바이트 (3번 레지스터 꽉 참)
    
    float2 uvOffset;   // 8바이트
    float2 uvScale;    // 8바이트 (여기까지 16바이트, 4번 레지스터 꽉 참)
    
    float spinAngle;   // 4바이트
    float3 pad_constants; // 12바이트 패딩 (마지막 16바이트 정렬 완벽)
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
    
    float3 scaledPos = input.position.xyz * Scale;
    float2 finalPos2D;
    
    if (Flag == 0)
    {
        float s = sin(Angle);
        float c = cos(Angle);
        
        finalPos2D.x = scaledPos.x * c - scaledPos.y * s;
        finalPos2D.y = scaledPos.x * s + scaledPos.y * c;
        
        finalPos2D += Offset.xy;
        finalPos2D.y -= cameraY;
        
    }
    else
    {
        finalPos2D = scaledPos.xy;
        finalPos2D += Offset.xy;
    }
    
    output.position = float4(finalPos2D, scaledPos.z, 1.0f);
    
    output.uv = (input.uv * uvScale) + uvOffset;
    output.color = Color;
    
    return output;
}



float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = uiTexture.Sample(uiSampler, input.uv);
    
    return texColor * input.color;
}
