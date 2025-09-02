#include "SimGameInstance.h"
#include "SimFirstRunSave.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"     // UGameUserSettings
#include "Engine/World.h"                       // FWorldDelegates::OnPostWorldInitialization
#include "HAL/IConsoleManager.h"                // IConsoleManager, IConsoleVariable

void USimGameInstance::Init()
{
    Super::Init();

    // First run: set scalability, lock res scale, disable dynamic res
    RunFirstLaunchGraphicsSetupIfNeeded();

    // Runtime: force TSR Quality & crisp output, apply desktop/native res
    EnforceRuntimeCvars();
    ApplyDesktopResolution();

    // Do it again after each map initializes (important for packaged builds)
    FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &USimGameInstance::OnPostWorldInit);
}

void USimGameInstance::OnPostWorldInit(UWorld* /*World*/, const UWorld::InitializationValues /*IVS*/)
{
    EnforceRuntimeCvars();
    ApplyDesktopResolution();
}

void USimGameInstance::RunFirstLaunchGraphicsSetupIfNeeded()
{
    const FString Slot = FirstRunSlot;

    USimFirstRunSave* Save = nullptr;
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
        Save = Cast<USimFirstRunSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Save)
        Save = Cast<USimFirstRunSave>(UGameplayStatics::CreateSaveGameObject(USimFirstRunSave::StaticClass()));
    if (!Save) return;

    UGameUserSettings* GS = (GEngine ? GEngine->GetGameUserSettings() : nullptr);
    if (!GS) return;

    bool bChanged = false;

    // First-run hardware benchmark to choose scalability
    if (!Save->bGraphicsBenchmarked)
    {
        GS->RunHardwareBenchmark();
        GS->ApplyHardwareBenchmarkResults();
        Save->bGraphicsBenchmarked = true;
        bChanged = true;
    }

    // Lock primary resolution scale at 100% so output/UI isn’t downscaled
    if (GS->GetResolutionScaleNormalized() != 1.0f)
    {
        GS->SetResolutionScaleValueEx(100); // 100%
        bChanged = true;
    }

    // Base AA quality preset (TSR uses temporal pipeline)
    GS->SetAntiAliasingQuality(3); // 0..3

    // Persistently disable Dynamic Resolution in user settings
    GS->SetDynamicResolutionEnabled(false);
    bChanged = true;

    if (bChanged)
    {
        GS->ConfirmVideoMode();
        GS->ApplySettings(false);
        GS->SaveSettings();
        UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    }
}

void USimGameInstance::ApplyDesktopResolution()
{
    UGameUserSettings* GS = (GEngine ? GEngine->GetGameUserSettings() : nullptr);
    if (!GS) return;

    // UE 5.5: non-static, no-arg member function
    const FIntPoint DesktopRes = GS->GetDesktopResolution();

    if (DesktopRes.X > 0 && DesktopRes.Y > 0)
    {
        if (GS->GetScreenResolution() != DesktopRes)
            GS->SetScreenResolution(DesktopRes);

        if (GS->GetFullscreenMode() != EWindowMode::WindowedFullscreen)
            GS->SetFullscreenMode(EWindowMode::WindowedFullscreen); // borderless at native

        GS->ConfirmVideoMode();
        GS->ApplySettings(false);
        GS->SaveSettings();
    }
}

void USimGameInstance::EnforceRuntimeCvars()
{
    // Use TSR as AA method (UE5: 0=None, 1=FXAA, 2=TAA, 3=MSAA, 4=TSR)
    if (IConsoleVariable* CVarAAMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
    {
        CVarAAMethod->Set(4, ECVF_SetByGameSetting);
    }

    // Ensure temporal upsampling path is active (TSR relies on it)
    if (IConsoleVariable* CVarTAAS = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAA.Upsampling")))
    {
        CVarTAAS->Set(1, ECVF_SetByGameSetting);
    }

    // Present at 100% output resolution (UI crisp)
    if (IConsoleVariable* CVarPrimarySP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
    {
        CVarPrimarySP->Set(100.0f, ECVF_SetByGameSetting);
    }

    // TSR "Quality" internal scale (~77% of output res)
    if (IConsoleVariable* CVarSecondarySP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport")))
    {
        CVarSecondarySP->Set(77, ECVF_SetByGameSetting);
    }

    // HARD OFF: Dynamic Resolution (prevents surprise downscaling in packaged)
    if (IConsoleVariable* CVarDynMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode")))
    {
        CVarDynMode->Set(0, ECVF_SetByGameSetting); // 0 = Off
    }
    if (IConsoleVariable* CVarDynMin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.MinScreenPercentage")))
    {
        CVarDynMin->Set(100.0f, ECVF_SetByGameSetting);
    }
    if (IConsoleVariable* CVarDynMax = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.MaxScreenPercentage")))
    {
        CVarDynMax->Set(100.0f, ECVF_SetByGameSetting);
    }

    // Optional mild sharpen (0.0–0.4) to counter residual softness
    if (IConsoleVariable* CVarSharpen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen")))
    {
        CVarSharpen->Set(0.2f, ECVF_SetByGameSetting);
    }

    // Make sure third-party upscalers are off (harmless if plugins missing)
    if (IConsoleVariable* CVarDLSS = IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.DLSS.Enable")))
    {
        CVarDLSS->Set(0, ECVF_SetByGameSetting);
    }
    if (IConsoleVariable* CVarFSR3 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR3.Enabled")))
    {
        CVarFSR3->Set(0, ECVF_SetByGameSetting);
    }
}
