#include "TimeLap.h"

TimeLap::TimeLap(FVector3 position, const std::string& textureName, int cols, int rows, float fullWidth, float fullHeight)
    : basePosition(position)
{
    for (int i = 0; i < 5; ++i)
    {
        FVector3 digitScale = FVector3(0.07f, 0.07f, 1.0f);

        Digit* newDigit = new Digit(FVector3(0, 0, 0), digitScale, textureName, cols, rows, fullWidth, fullHeight);
        digits.push_back(newDigit);
    }

    lastTimeStr = "00:00";
    for (int i = 0; i < 5; ++i)
    {
        digits[i]->SetChar(lastTimeStr[i]);
    }
}

// 소멸자: 동적 할당된 Digit 객체들을 안전하게 삭제합니다.
TimeLap::~TimeLap()
{
    for (auto digit : digits)
    {
        if (digit != nullptr)
        {
            delete digit;
            digit = nullptr;
        }
    }
    digits.clear();
}

void TimeLap::Update(float deltaTime)
{
    totalTime += deltaTime;
    
    int minutes = (int)(totalTime / 60) % 100;
    int seconds = (int)totalTime % 60;

    char buf[6];
    sprintf_s(buf, "%02d:%02d", minutes, seconds);
    std::string timeStr = buf;

    if (lastTimeStr != timeStr)
    {
        for (int i = 0; i < 5; ++i)
        {
            digits[i]->SetChar(timeStr[i]);
        }
        lastTimeStr = timeStr;
    }


}

void TimeLap::Render(URenderer& renderer)
{
    if (GetActive() == false)
    {
        return;
    }
    float startX = basePosition.x - (digitSpacing * 2.0f);
    float currentX = startX;

    for (int i = 0; i < digits.size(); ++i)
    {
        digits[i]->SetPosition(currentX, basePosition.y);

        digits[i]->Render(renderer);

        currentX += digitSpacing;
    }
}

void TimeLap::Clear()
{
    totalTime = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        digits[i]->SetChar(defaultTimeStr[i]);
    }
}