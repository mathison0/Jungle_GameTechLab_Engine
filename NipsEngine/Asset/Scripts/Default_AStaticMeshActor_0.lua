function BeginPlay(owner)
    print("BeginPlay", owner:GetName())

    StartCoroutine(function()
        print("coroutine start")

        wait(1.0)
        print("after 1 sec")

        wait(2.0)
        print("after 3 sec total")
    end)
end

function Tick(owner, deltaTime)
end