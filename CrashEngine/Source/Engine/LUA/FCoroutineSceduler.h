#pragma once
#include "Core/CoreTypes.h"

struct FLuaCoroutineTask
{
    sol::thread Thread;
    sol::coroutine Coroutine;
};


class FLuaCoroutineUtil
{
    static FLuaCoroutineTask StartCoroutine(const std::string& FunctionName)
    {
        sol::function LuaFunc = Lua[FunctionName];

        if (!LuaFunc.valid())
        {
            ULog("Lua function not found: %s\n", FunctionName.c_str());
            return;
        }

        FLuaCoroutineTask Task;

        Task.Name = FunctionName;

        // Lua VM 안에 코루틴용 thread 생성
        Task.Thread = sol::thread::create(Lua);

        // thread 내부 state에 접근하기 위한 view
        sol::state_view ThreadState = Task.Thread.state();

        // 메인 Lua state에 있던 함수를 thread 쪽에 꽂음
        ThreadState[FunctionName] = LuaFunc;

        // thread 안의 함수를 coroutine으로 감쌈
        Task.Coroutine = sol::coroutine(ThreadState[FunctionName]);

        Task.YieldType = ELuaYieldType::None;
        Task.bFinished = false;
        Task.bError = false;
        Task.bCancelled = false;

        return Task;
    }

    static void Tick(float DeltaTime, FLuaCoroutineTask& Task)
    {
        sol::protected_function_result Result;

        Result = Task.Coroutine();

        if (!Result.valid())
        {
            sol::error Err = Result;

            printf("Lua Coroutine Error [%s]: %s\n",
                   Task.Name.c_str(),
                   Err.what());

            Task.bError = true;
            Task.YieldType = ELuaYieldType::Error;
            return;
        }

        if (Result.status() == sol::call_status::yielded)
        {
            ProcessYieldResult(Task, Result);
            return;
        }

        Task.bFinished = true;
        Task.YieldType = ELuaYieldType::Finished;
    }

    static void ResumeCoroutine(FLuaCoroutineTask& Task)
    {
        sol::protected_function_result Result = Task.Coroutine();

        if (!Result.valid())
        {
            Task.State = ELuaCoroutineState::Error;
            return;
        }

        if (Result.status() == sol::call_status::yielded)
        {
            std::string Command = Result.get<std::string>(0);

            if (Command == "wait_time")
            {
                float Seconds = Result.get<float>(1);

                Task.State = ELuaCoroutineState::WaitTime;
                Task.RemainingTime = Seconds;
                return;
            }

            if (Command == "wait_event")
            {
                std::string EventName = Result.get<std::string>(1);

                Task.State = ELuaCoroutineState::WaitEvent;
                Task.EventName = EventName;
                return;
            }

            Task.State = ELuaCoroutineState::Error;
            return;
        }

        Task.State = ELuaCoroutineState::Finished;
    }
};

struct CouroutineContext
{
    float DeltaTime;
    float FixedDeltaTime;
};

class ExecuteCommandBase
{
public:
    virtual void Run(sol::coroutine& InCoroutine, CouroutineContext& Context) = 0;
    virtual bool IsEnd() = 0;
    virtual virtual sol::protected_function_result GetResult() = 0;
};

class ExecuteCommand : public ExecuteCommandBase
{
public:
    virtual void Run(sol::coroutine& InCoroutine, CouroutineContext& Context) { Result = InCoroutine(); }
    virtual bool IsEnd() { return true; }
    sol::protected_function_result GetResult() { return Result; }

protected:
    sol::protected_function_result Result;
};


class WaitCommandRealTime final : public ExecuteCommand
{
public:
    WaitCommandRealTime(float RealTime) { TargetTime = RealTime; }
    void Run(sol::coroutine& InCoroutine, CouroutineContext& Context) override;
    virtual bool IsEnd() override { return CurrentTime >= TargetTime; }


private:
    float TargetTime = 0;
    float CurrentTime = 0;
};

class CoroutineExecutor
{
public:
    CoroutineExecutor(FLuaCoroutineTask&& InTask) : Task(std::move(InTask)) {}
    ~CoroutineExecutor() { delete Command; }
    bool Tick(CouroutineContext& Context);
    bool IsValid();

private:
    void SetCommand(ExecuteCommand* NewCommand);

private:
    ExecuteCommand* Command = nullptr;
    FLuaCoroutineTask Task;
};

class CouroutineSet
{
    void Start(const std::string& FunctionName);
    void Tick(CouroutineContext& Context);

    TArray<CoroutineExecutor*> Executors;
};