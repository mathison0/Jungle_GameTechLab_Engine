local BGM_KEY = "MainBGM"
local BGM_PATH = "Main_bgm.mp3"

function BeginPlay()
    if AudioManager.Load(BGM_KEY, BGM_PATH, true) then
        AudioManager.PlayBGM(BGM_KEY, AudioManager.GetBGMVolume())
    else
        print("[MainBGM] failed to load " .. BGM_PATH)
    end
end

function EndPlay()
    AudioManager.StopBGM()
end
