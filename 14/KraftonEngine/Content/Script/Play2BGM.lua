local BGM_KEY = "Play2BGM"
local BGM_PATH = "Play2_bgm.mp3"

function BeginPlay()
    if AudioManager.Load(BGM_KEY, BGM_PATH, true) then
        AudioManager.PlayBGM(BGM_KEY, AudioManager.GetBGMVolume())
    else
        print("[Play2BGM] failed to load " .. BGM_PATH)
    end
end

function EndPlay()
    AudioManager.StopBGM()
end
