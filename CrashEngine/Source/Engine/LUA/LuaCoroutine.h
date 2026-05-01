#pragma once
#include "Core/CoreTypes.h"
#include "Sol/sol.hpp"

struct FLuaCoroutineTask
{
    sol::thread Thread;
    sol::coroutine Coroutine;
};

struct FCouroutineContext
{
    float DeltaTime = 0.f;
    float FixedDeltaTime = 0.f;

    FCouroutineContext(float InDeltaTime = 0.f, float InFixedDeltaTime = 1 / 50)
    {
        DeltaTime = InDeltaTime;
        FixedDeltaTime = InFixedDeltaTime;
    }
};

class FExecuteCommandBase
{
public:
    virtual void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) = 0;
    virtual bool IsEnd() = 0;
    virtual bool IsValid() = 0;
    virtual sol::call_status GetStatus() = 0;
    void ParseResult(sol::call_status);
};

class FExecuteCommand : public FExecuteCommandBase
{
public:
    virtual void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) { ParseResult(InCoroutine()); }
    virtual bool IsEnd() { return true; }
    virtual bool IsValid() override { return bIsValid; };
    virtual sol::call_status GetStatus() override { return Status; };
    void ParseResult(sol::protected_function_result InResult);

    template <typename T>
    T GetResultParam(uint32 index) { return Args[index].as<T>(); }

protected:
    bool bIsValid = true;
    sol::call_status Status;
    TArray<sol::object> Args;
};


class FWaitCommandRealTime final : public FExecuteCommand
{
public:
    FWaitCommandRealTime(float RealTime) { TargetTime = RealTime; }
    void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) override;
    virtual bool IsEnd() override { return CurrentTime >= TargetTime; }


private:
    float TargetTime = 0;
    float CurrentTime = 0;
};

class FWaitNextFrame final : public FExecuteCommand
{
public:
    void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) override;
};

class FWaitUntilPredicate final : public FExecuteCommand
{
public:
    FWaitUntilPredicate(sol::function InPredicate) { Predicate = InPredicate; }
    void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) override { ParseResult(Predicate()); }
    virtual bool IsEnd() override { return GetResultParam<bool>(0); }

private:
    sol::function Predicate;
};

class CoroutineExecutor
{
public:
    CoroutineExecutor(FLuaCoroutineTask&& InTask)
        : Task(std::move(InTask)) { SetCommand(new FExecuteCommand()); }
    ~CoroutineExecutor() { delete Command; }
    void Tick(const FCouroutineContext& Context);
    bool IsValid();

private:
    void SetCommand(FExecuteCommand* NewCommand);

private:
    FExecuteCommand* Command = nullptr;
    FLuaCoroutineTask Task;
};

class FCoroutineExecutorSet
{
public:
    bool Start(const sol::function& LuaFunc);
    bool Stop(const sol::function& LuaFunc);
    void Tick(const FCouroutineContext& Context);

private:
    TMap<const void*, CoroutineExecutor*> Executors;
    TSet<const void*> PendingStopKeys;
    uint32 NextExecutorId = 0;
    bool bIsTicking = false;
};
