Texture2D currentTexture : register(t0); // 이름 변경 (더 일반적으로)
SamplerState textureSampler : register(s0);

cbuffer Constants : register(b0)
{
    float3 Offset;
    float Angle;
};

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float3 scaledPos = input.position.xyz * Offset.z;
    float s = sin(Angle);
    float c = cos(Angle);
    float2 rotatedPos;
    rotatedPos.x = scaledPos.x * c - scaledPos.y * s;
    rotatedPos.y = scaledPos.x * s + scaledPos.y * c;
    
    output.position = float4(rotatedPos.x + Offset.x, rotatedPos.y + Offset.y, 0, input.position.w);
    output.color = input.color;
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    const float PI = 3.14159265f;
    float2 p = input.uv * 2.0f - 1.0f;
    p.y = -p.y;
    float r2 = p.x * p.x + p.y * p.y;
    
    if (r2 > 1.0f)
        discard; // 원 밖은 그리지 않음
    
    float z = sqrt(max(0.0f, 1.0f - r2));
    float sphereU = 0.5f + (atan2(p.x, z) / (2.0f * PI));
    float sphereV = 0.5f - (asin(p.y) / PI);

    // register(t0)에 바인딩된 텍스처 사용 (어떤 텍스처든 상관없음)
    return currentTexture.Sample(textureSampler, float2(sphereU, sphereV));
}