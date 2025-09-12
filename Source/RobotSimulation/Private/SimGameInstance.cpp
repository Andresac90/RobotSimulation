#include "SimGameInstance.h"
#include "SimFirstRunSave.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"

void USimGameInstance::Init()
{
    Super::Init();

    // One-time graphics setup.
    RunFirstLaunchGraphicsSetupIfNeeded();
}

void USimGameInstance::RunFirstLaunchGraphicsSetupIfNeeded()
{
    const FString Slot = FirstRunSlot;

    // Load or create the save that tracks first-run completion.
    USimFirstRunSave* Save = nullptr;
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        Save = Cast<USimFirstRunSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    }
    if (!Save)
    {
        Save = Cast<USimFirstRunSave>(UGameplayStatics::CreateSaveGameObject(USimFirstRunSave::StaticClass()));
    }
    if (!Save) return;

    // If we've already applied settings once, we're done.
    if (Save->bFirstRunApplied)
    {
        return;
    }

    UGameUserSettings* GS = (GEngine ? GEngine->GetGameUserSettings() : nullptr);
    if (!GS) return;

    // --- FORCE EPIC SCALABILITY (no benchmark, no checks) ---
    {
        Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
        Q.SetFromSingleQualityLevel(3);  // 0=Low .. 3=Epic
        Q.ResolutionQuality = 70.0f;     // lock to 70% primary resolution scale
        Scalability::SetQualityLevels(Q);
    }

    // --- BORDERLESS FULLSCREEN @ DESKTOP/NATIVE ---
    {
        const FIntPoint DesktopRes = GS->GetDesktopResolution();   // UE 5.5 non-static
        if (DesktopRes.X > 0 && DesktopRes.Y > 0)
        {
            GS->SetScreenResolution(DesktopRes);
        }
        GS->SetFullscreenMode(EWindowMode::WindowedFullscreen);
    }

    // --- FIX PRIMARY RESOLUTION SCALE to 70% (persisted in GameUserSettings.ini) ---
    GS->SetResolutionScaleValueEx(70); // 70%

    // --- ENABLE TSR once (requires TAA path) ---
    if (IConsoleVariable* CVarAAMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AntiAliasing")))
    {
        // 0=None, 1=FXAA, 2=TAA, 3=MSAA
        CVarAAMethod->Set(2, ECVF_SetByGameSetting);
    }
    if (IConsoleVariable* CVarUpsample = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAA.Upsampling")))
    {
        CVarUpsample->Set(1, ECVF_SetByGameSetting); // TSR path on
    }

    // Optional: align TSR internal percentage (harmless if CVar doesn't exist).
    if (IConsoleVariable* CVarSecondarySP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport")))
    {
        CVarSecondarySP->Set(70, ECVF_SetByGameSetting);
    }

    // Apply & persist (first run only)
    GS->ApplyNonResolutionSettings();
    GS->ApplyResolutionSettings(false);
    GS->ConfirmVideoMode();
    GS->SaveSettings();

    // Mark as done; never touch again.
    Save->bFirstRunApplied = true;
    UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
}
