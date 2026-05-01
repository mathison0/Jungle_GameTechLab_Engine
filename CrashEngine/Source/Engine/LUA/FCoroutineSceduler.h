#pragma once
#include "Core/CoreTypes.h"

struct FLuaCoroutineTask
{
    sol::thread Thread;
    sol::coroutine Coroutine;
};

struct FCouroutineContext
{
    float DeltaTime;
    float FixedDeltaTime;
};

class FExecuteCommandBase
{
public:
    virtual void Run(sol::coroutine& InCoroutine, FCouroutineContext& Context) = 0;
    virtual bool IsEnd() = 0;
    virtual virtual sol::protected_function_result GetResult() = 0;
};

class FExecuteCommand : public FExecuteCommandBase
{
public:
    virtual void Run(sol::coroutine& InCoroutine, FCouroutineContext& Context) { Result = InCoroutine(); }
    virtual bool IsEnd() { return true; }
    sol::protected_function_result GetResult() { return Result; }

protected:
    sol::protected_function_result Result;
};


class FWaitCommandRealTime final : public FExecuteCommand
{
public:
    FWaitCommandRealTime(float RealTime) { TargetTime = RealTime; }
    void Run(sol::coroutine& InCoroutine, FCouroutineContext& Context) override;
    virtual bool IsEnd() override { return CurrentTime >= TargetTime; }


private:
    float TargetTime = 0;
    float CurrentTime = 0;
};

class FWaitNextFrame final : public FExecuteCommand
{
    void Run(sol::coroutine& InCoroutine, FCouroutineContext& Context) override;
};

class CoroutineExecutor
{
public:
    CoroutineExecutor(FLuaCoroutineTask&& InTask)
        : Task(std::move(InTask)) { SetCommand(new FExecuteCommand()); }
    ~CoroutineExecutor() { delete Command; }
    bool Tick(FCouroutineContext& Context);
    bool IsValid();

private:
    void SetCommand(FExecuteCommand* NewCommand);

private:
    FExecuteCommand* Command = nullptr;
    FLuaCoroutineTask Task;
};

class CoroutineExecutorSet
{
    void Start(const FString& FunctionName);
    void Stop(const FString& FunctionName);
    void Tick(FCouroutineContext& Context);

    TMap<FString, CoroutineExecutor*> Executors;
};