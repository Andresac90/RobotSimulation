#include "SimGameInstance.h"
#include "SimFirstRunSave.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"

static void SetCVarInt(const TCHAR* Name, int32 Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByGameSetting);
    }
}

static void SetCVarFloat(const TCHAR* Name, float Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByGameSetting);
    }
}

void USimGameInstance::Init()
{
    Super::Init();
    RunFirstLaunchGraphicsSetupIfNeeded();
}

void USimGameInstance::RunFirstLaunchGraphicsSetupIfNeeded()
{
    const FString Slot = FirstRunSlot;

    // Load/create SaveGame tracking first-run completion
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

    // Already applied? Bail.
    if (Save->bFirstRunApplied)
    {
        return;
    }

    UGameUserSettings* GS = (GEngine ? GEngine->GetGameUserSettings() : nullptr);
    if (!GS) return;

    // --------------------------------------------------------------------
    // 1) Auto-detect hardware and apply recommended scalability levels
    // --------------------------------------------------------------------
    GS->RunHardwareBenchmark();
    GS->ApplyHardwareBenchmarkResults();

    // Prefer Epic view distance as baseline (we'll further boost via CVars)
    GS->SetViewDistanceQuality(3);

    // --------------------------------------------------------------------
    // 2) Borderless fullscreen at desktop native (keeps device aspect ratio)
    // --------------------------------------------------------------------
    {
        const FIntPoint DesktopRes = GS->GetDesktopResolution();
        if (DesktopRes.X > 0 && DesktopRes.Y > 0)
        {
            GS->SetScreenResolution(DesktopRes);
        }
        GS->SetFullscreenMode(EWindowMode::WindowedFullscreen);
    }

    // --------------------------------------------------------------------
    // 3) Native pixel density: primary & secondary = 100%
    // --------------------------------------------------------------------
    GS->SetResolutionScaleValueEx(100);                  // primary resolution scale
    SetCVarInt(TEXT("r.SecondaryScreenPercentage.GameViewport"), 100);
    SetCVarInt(TEXT("r.ScreenPercentage"), 100);

    // --------------------------------------------------------------------
    // 4) Enable TSR and set TSR Quality
    // --------------------------------------------------------------------
    // Ensure TAA/TSR path (0=None, 1=FXAA, 2=TAA, 3=MSAA)
    SetCVarInt(TEXT("r.DefaultFeature.AntiAliasing"), 2);

    // Turn on temporal upsampling path (TSR/TAAU)
    SetCVarInt(TEXT("r.TemporalAA.Upsampling"), 1);

    // Prefer TSR if available (harmless no-op if absent)
    SetCVarInt(TEXT("r.UpScaling"), 1);

    // TSR quality preset (0=Low,1=Medium,2=High,3=Quality/Epic)
    SetCVarInt(TEXT("r.TSR.Quality"), 3);

    // Keep aspect behavior consistent (0=MaintainYFOV)
    SetCVarInt(TEXT("r.AspectRatioAxisConstraint"), 0);

    // --------------------------------------------------------------------
    // 5) Maximize Landscape draw distance / detail at range
    // --------------------------------------------------------------------
    // Global view distance multiplier (affects more than Landscape, use judiciously)
    SetCVarFloat(TEXT("r.ViewDistanceScale"), 2.0f);

    // Landscape-specific: push higher-detail LODs further out
    // Larger distribution scales shift LOD transitions farther; negative bias prefers higher-detail LODs.
    SetCVarFloat(TEXT("r.LandscapeLOD0DistributionScale"), 3.0f);
    SetCVarFloat(TEXT("r.LandscapeLODDistributionScale"), 3.0f);
    SetCVarInt(TEXT("r.LandscapeLODBias"), -2);

    // If using World Partition/HLOD and seeing early swaps, you can relax HLOD (optional, uncomment if needed)
    // SetCVarInt(TEXT("r.HLOD"), 0);

    // --------------------------------------------------------------------
    // 6) Apply and persist to disk
    // --------------------------------------------------------------------
    GS->ApplyNonResolutionSettings();
    GS->ApplyResolutionSettings(false);
    GS->ConfirmVideoMode();
    GS->SaveSettings();

    // Mark as done (first run only)
    Save->bFirstRunApplied = true;
    UGameplayStatics::SaveGameToSlot(Save, Slot, 0);

#if !UE_BUILD_SHIPPING
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("First-run graphics configuration applied."));
    }
#endif
}
