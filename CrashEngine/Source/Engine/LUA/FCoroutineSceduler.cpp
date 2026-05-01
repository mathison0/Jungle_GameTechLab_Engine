#include "FCoroutineSceduler.h"

void WaitCommandRealTime::Run(sol::coroutine& InCoroutine, CouroutineContext& Context)
{
    CurrentTime += Context.DeltaTime;

    if (CurrentTime >= TargetTime)
        Result = InCoroutine();
}


void CoroutineExecutor::SetCommand(ExecuteCommand* NewCommand)
{
    if (Command)
        delete Command;

    Command = NewCommand;
}

void CoroutineExecutor::Tick(CouroutineContext& Context)
{
    if (!Command)
        SetCommand(new ExecuteCommand());

    Command->Run(Task.Coroutine, Context);

    if (!Command->IsEnd())
        return;

    auto Result = Command->GetResult();
    if (!Result.valid())
        return;

    if (Result.status() == sol::call_status::yielded)
    {
        std::string Command = Result.get<std::string>(0);

        if (Command == "wait_time")
        {
            float TargetTime = Result.get<float>(1);
            SetCommand(new WaitCommandRealTime(TargetTime));
        }
    }
}

bool CoroutineExecutor::IsValid()
{
    if (!Command->IsEnd())
        return true;

    auto Result = Command->GetResult();
    if (!Result.valid())
        return false;

    if (Result.status() == sol::call_status::yielded)
        return true;

	return false;
}

void CouroutineSet::Start(const std::string& FunctionName)
{
    sol::function LuaFunc = Lua[FunctionName];

    if (!LuaFunc.valid())
    {
        //UE_Log("Lua function not found: %s\n", FunctionName.c_str());
        return;
    }

    FLuaCoroutineTask Task;

    // Lua VM 안에 코루틴용 thread 생성
    Task.Thread = sol::thread::create(Lua);

    // thread 내부 state에 접근하기 위한 view
    sol::state_view ThreadState = Task.Thread.state();

    // 메인 Lua state에 있던 함수를 thread 쪽에 꽂음
    ThreadState[FunctionName] = LuaFunc;

    // thread 안의 함수를 coroutine으로 감쌈
    Task.Coroutine = sol::coroutine(ThreadState[FunctionName]);


    Executors.push_back(new CoroutineExecutor(std::move(Task)));
}

void CouroutineSet::Tick(CouroutineContext& Context)
{
	for (auto& Executor : Executors)
	{
        Executor->Tick(Context);
	}

	int ExecutorCount = Executors.size();
	for (int i = 0; i < ExecutorCount; i++)
	{
        if (Executors[i]->IsValid())
            continue;


	}
}
