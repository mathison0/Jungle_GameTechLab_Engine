#pragma once
#include "Digit.h"
#include <vector>
#include <string>

class TimeLap
{
private:
    std::vector<Digit*> digits; 
    float totalTime = 0.0f;
    std::string lastTimeStr = "";

    FVector3 basePosition;      
    float digitSpacing = 0.05f;  

public:
    TimeLap(FVector3 position, const std::string& textureName, int cols, int rows, float fullWidth, float fullHeight);
    ~TimeLap();

    void Update(float deltaTime);
    void Render(URenderer& renderer);

    // 유틸리티
    void SetPosition(FVector3 pos) { basePosition = pos; }
    float GetTotalWidth();
};