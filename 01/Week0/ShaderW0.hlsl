Texture2D earthTexture : register(t0);
Texture2D pngTexture : register(t1);
SamplerState earthSampler : register(s0);

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
    float3 padding; // 16바이트 정렬을 위한 패딩
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
    
    //scale, rotation, translation
    float3 scaledPos = input.position.xyz * Offset.z;
    float s = sin(Angle);
    float c = cos(Angle);
    float2 rotatedPos;
    rotatedPos.x = scaledPos.x * c - scaledPos.y * s;
    rotatedPos.y = scaledPos.x * s + scaledPos.y * c;
    
    //to pixel shader
    output.position = float4(rotatedPos.x + Offset.x, rotatedPos.y + Offset.y, 0, input.position.w);

    output.position.y -= cameraY; // 카메라 Y 위치 적용
    
    // 색상을 픽셀 셰이더로 전달
    output.color = input.color;
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    if (Flag == 0)
    {
        return Color;
    }
    
    float brightness = Color.w;
    // 1. UV가 음수일 경우 (새로운 PNG 텍스처 사용)
    if (input.uv.x < 0.0f || input.uv.y < 0.0f)
    {
        float2 correctUV = abs(input.uv);
        
        // UV를 중심(0.5, 0.5) 기준으로 회전
        float2 centered = correctUV - 0.5f;
        float s = sin(Angle);
        float c = cos(Angle);
        float2 rotatedUV;
        rotatedUV.x = centered.x * c - centered.y * s;
        rotatedUV.y = centered.x * s + centered.y * c;
        rotatedUV += 0.5f; // 다시 중심으로 이동
        
        return pngTexture.Sample(earthSampler, rotatedUV) * brightness;
    }
    //UV가 (0,0)이면 vertex color 사용 (cube용) -> Sphere.h에서 uv 좌표 수정
    //UV가 (0,0)이 아니면 텍스처 사용 (sphere용)
    else if (input.uv.x == 0.0f && input.uv.y == 0.0f)
    {
        return input.color * brightness; // Vertex color 사용
    }
    //2d texture mapping
    //texture에서 그대로 잘라 넣지 않고 구의 표면에 맞게 uv 좌표 변형.
    const float PI = 3.14159265f;
    float2 p = input.uv * 2.0f - 1.0f;
    p.y = -p.y;
    float r2 = p.x * p.x + p.y * p.y;
    float z = sqrt(max(0.0f, 1.0f - r2));
    
    // === 텍스처 회전 추가 ===
    float s = sin(Angle);
    float c = cos(Angle);
    float2 rotatedP;
    rotatedP.x = p.x * c - p.y * s;
    rotatedP.y = p.x * s + p.y * c;
    
    float sphereU = 0.5f + (atan2(rotatedP.x, z) / (2.0f * PI));
    float sphereV = 0.5f - (asin(rotatedP.y) / PI);

    sphereU = frac(sphereU + spinAngle);
    //왜곡된 새 UV로 텍스처를 샘플링
    return earthTexture.Sample(earthSampler, float2(sphereU, sphereV)) * brightness;
}