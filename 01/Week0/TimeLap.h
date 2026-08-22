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
    std::string defaultTimeStr = "00:00";

    FVector3 basePosition;      
    float digitSpacing = 0.05f;  
    bool isActive = false;

public:
    TimeLap(FVector3 position, const std::string& textureName, int cols, int rows, float fullWidth, float fullHeight);
    ~TimeLap();

    void Update(float deltaTime);
    void Render(URenderer& renderer);

    void SetActive(bool active) { isActive = active; }
    bool GetActive() { return isActive; }

    void SetPosition(FVector3 pos) { basePosition = pos; }

    void Clear();
};