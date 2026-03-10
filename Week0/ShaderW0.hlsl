Texture2D earthTexture : register(t0);
SamplerState earthSampler : register(s0);

cbuffer Constants : register(b0)
{
    float3 Offset; // x: 위치X, y: 위치Y, z: 스케일(반지름)
    float Angle; // C++에서 넘겨준 회전 각도
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
    output.color = input.color;
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    //UV가 (0,0)이면 vertex color 사용 (cube용) -> Sphere.h에서 uv 좌표 수정
    //UV가 (0,0)이 아니면 텍스처 사용 (sphere용)
    if (input.uv.x == 0.0f && input.uv.y == 0.0f)
    {
        return input.color; // Vertex color 사용
    }
    //2d texture mapping
    //texture에서 그대로 잘라 넣지 않고 구의 표면에 맞게 uv 좌표 변형.
    const float PI = 3.14159265f;
    float2 p = input.uv * 2.0f - 1.0f;
    p.y = -p.y;
    float r2 = p.x * p.x + p.y * p.y;
    float z = sqrt(max(0.0f, 1.0f - r2));
    float sphereU = 0.5f + (atan2(p.x, z) / (2.0f * PI));
    float sphereV = 0.5f - (asin(p.y) / PI);

    //왜곡된 새 UV로 텍스처를 샘플링
    return earthTexture.Sample(earthSampler, float2(sphereU, sphereV));
}