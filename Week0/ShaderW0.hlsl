cbuffer Constants : register(b0)
{
    float3 Offset; // x: 위치X, y: 위치Y, z: 스케일(반지름)
    float Angle; // C++에서 넘겨준 회전 각도
};

struct VS_INPUT
{
    float4 position : POSITION; // Input position from vertex buffer
    float4 color : COLOR; // Input color from vertex buffer
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 color : COLOR; // Color to pass to the pixel shader
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    // 1. 스케일(Scale) 적용
    float3 scaledPos = input.position.xyz * Offset.z;
    
    // 2. 회전(Rotation) 적용 (Z축 기준 2D 회전)
    float s = sin(Angle);
    float c = cos(Angle);
    
    float2 rotatedPos;
    rotatedPos.x = scaledPos.x * c - scaledPos.y * s;
    rotatedPos.y = scaledPos.x * s + scaledPos.y * c;
    
    // 3. 이동(Translation) 적용
    output.position = float4(rotatedPos.x + Offset.x, rotatedPos.y + Offset.y, 0, input.position.w);
    
    // 색상을 픽셀 셰이더로 전달
    output.color = input.color;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Output the color directly
    return input.color;
}