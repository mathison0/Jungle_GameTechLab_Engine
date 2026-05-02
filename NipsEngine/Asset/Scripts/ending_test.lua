local KEY_F5 = 0x74
local triggered = false

function BeginPlay(owner)
    Log("[EndingTest] BeginPlay - 스크립트 로드됨")
end

function Tick(owner, deltaTime)
    if triggered then return end

    -- F5 체크
    if GetKeyDown(KEY_F5) then
        Log("[EndingTest] F5 감지됨!")
        triggered = true
        StartCoroutine(TriggerEnding)
    end

    -- P 키로도 같이 테스트
    if GetKeyDown(0x50) then
        Log("[EndingTest] P키 감지됨!")
    end
end