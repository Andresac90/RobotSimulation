// SimGameInstance.cpp
#include "SimGameInstance.h"
#include "SimFirstRunSave.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericApplication.h"   // FDisplayMetrics
#include "HAL/IConsoleManager.h"

void USimGameInstance::Init()
{
    Super::Init();
    RunFirstLaunchGraphicsSetupIfNeeded();
}

void USimGameInstance::RunFirstLaunchGraphicsSetupIfNeeded()
{
    const FString Slot = FirstRunSlot;

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

    UGameUserSettings* GS = (GEngine ? GEngine->GetGameUserSettings() : nullptr);
    if (!GS) return;

    bool bChangedAnything = false;

    // 1) Run hardware benchmark (only once)
    if (!Save->bGraphicsBenchmarked)
    {
        GS->RunHardwareBenchmark();
        GS->ApplyHardwareBenchmarkResults();
        Save->bGraphicsBenchmarked = true;
        bChangedAnything = true;
    }

    // 2) Set to desktop/native resolution + borderless fullscreen (once)
    if (!Save->bResolutionConfigured)
    {
        FDisplayMetrics DM;
        FDisplayMetrics::RebuildDisplayMetrics(DM);

        const FIntPoint DesktopRes(DM.PrimaryDisplayWidth, DM.PrimaryDisplayHeight);
        if (DesktopRes.X > 0 && DesktopRes.Y > 0)
        {
            GS->SetScreenResolution(DesktopRes);
            GS->SetFullscreenMode(EWindowMode::WindowedFullscreen); // borderless at native res
            Save->bResolutionConfigured = true;
            bChangedAnything = true;
        }
    }

    // 3) Enable TSR upscaling + set screen percentage (once)
    if (!Save->bTSRConfigured)
    {
        // Pick a sensible default. 77 ~= “Quality” TSR (good visual/perf balance).
        // You can expose this as a project setting later if you want.
        const int32 TargetSP = 77;

        // Persisted in GameUserSettings.ini:
        GS->SetResolutionScaleValueEx(TargetSP);

        // Make sure TSR upsampling is actually used this session:
        if (IConsoleVariable* CVarTAAS = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAA.Upsampling")))
        {
            CVarTAAS->Set(1, ECVF_SetByGameSetting);
        }
        // Keep UI crisp (primary at 100) and upscale 3D via the secondary percentage:
        if (IConsoleVariable* CVarSecSP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport")))
        {
            CVarSecSP->Set(TargetSP, ECVF_SetByGameSetting);
        }
        if (IConsoleVariable* CVarPrimarySP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
        {
            CVarPrimarySP->Set(100.0f, ECVF_SetByGameSetting);
        }

        Save->bTSRConfigured = true;
        bChangedAnything = true;
    }

    // Apply & persist
    if (bChangedAnything)
    {
        GS->ConfirmVideoMode();
        GS->ApplySettings(false);
        GS->SaveSettings();

        UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    }
}
