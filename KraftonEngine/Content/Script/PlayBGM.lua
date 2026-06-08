local BGM_KEY = "Play1BGM"
local BGM_PATH = "Play1_bgm.wav"

function BeginPlay()
    if AudioManager.Load(BGM_KEY, BGM_PATH, true) then
        AudioManager.PlayBGM(BGM_KEY, AudioManager.GetBGMVolume())
    else
        print("[PlayBGM] failed to load " .. BGM_PATH)
    end
end

function EndPlay()
    AudioManager.StopBGM()
end
