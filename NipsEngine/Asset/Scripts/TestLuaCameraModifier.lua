local Time = 0.0

function ModifyCamera(View, DeltaTime)
    Time = Time + DeltaTime

    -- FOV is stored in radians. Keep the offset small so the test is visible but not extreme.
    View.FOV = View.FOV + math.sin(Time * 4.0) * 0.03

    return true
end

function ModifyPostProcess(Settings, DeltaTime)
    Settings.Gamma = 1.0 + (math.sin(Time * 2.0) * 0.5 + 0.5) * 1.4
    Settings.VignetteIntensity = 1.0
    Settings.VignetteRadius = 0.35
    Settings.VignetteSoftness = 0.45

    return true
end

function ModifyOverlay(Overlay, DeltaTime)
    Overlay.LetterBoxRatio = 0.08
    Overlay.FadeColor = FVector4(0.0, 0.0, 0.0, 0.15)

    return true
end
