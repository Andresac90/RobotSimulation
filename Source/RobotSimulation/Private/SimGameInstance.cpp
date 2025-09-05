#include "SimGameInstance.h"
#include "SimFirstRunSave.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"     // UGameUserSettings
#include "Engine/World.h"                       // FWorldDelegates::OnPostWorldInitialization
#include "HAL/IConsoleManager.h"                // IConsoleManager, IConsoleVariable
#include "TimerManager.h"
#include "Containers/Ticker.h"                  // FTSTicker

void USimGameInstance::Init()
{
    Super::Init();

    // One-time (per slot) scalability + persistent user settings
    RunFirstLaunchGraphicsSetupIfNeeded();

    // Enforce runtime CVars now
    EnforceRuntimeCvars();

    // Run after each map init (works in editor & packaged)
    FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &USimGameInstance::OnPostWorldInit);

    // Also verify on the first tick after startup (in case viewport isn’t ready yet)
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &USimGameInstance::TickerVerify), 0.10f);
}

bool USimGameInstance::TickerVerify(float /*DeltaSeconds*/)
{
    VerifyAndForceOutputResolution();
    // return false to run once
    return false;
}

void USimGameInstance::OnPostWorldInit(UWorld* World, const UWorld::InitializationValues /*IVS*/)
{
    EnforceRuntimeCvars();

    if (World)
    {
        // Let the viewport settle for one tick then verify
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(this, &USimGameInstance::VerifyAndForceOutputResolution));
    }
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

    // First-run hardware benchmark to set scalability.
    if (!Save->bGraphicsBenchmarked)
    {
        GS->RunHardwareBenchmark();
        GS->ApplyHardwareBenchmarkResults();
        Save->bGraphicsBenchmarked = true;
        bChanged = true;
    }

    // Always prefer borderless fullscreen at desktop/native res in user settings.
    {
        const FIntPoint DesktopRes = GS->GetDesktopResolution();   // non-static in UE5.5
        if (DesktopRes.X > 0 && DesktopRes.Y > 0 && GS->GetScreenResolution() != DesktopRes)
        {
            GS->SetScreenResolution(DesktopRes);
            bChanged = true;
        }

        if (GS->GetFullscreenMode() != EWindowMode::WindowedFullscreen)
        {
            GS->SetFullscreenMode(EWindowMode::WindowedFullscreen);
            bChanged = true;
        }
    }

    // Lock primary resolution scale at 100% so the output/UI is crisp.
    if (!FMath::IsNearlyEqual(GS->GetResolutionScaleNormalized(), 1.0f))
    {
        GS->SetResolutionScaleValueEx(100);
        bChanged = true;
    }

    // Base AA method quality (TSR builds on TAA).
    GS->SetAntiAliasingQuality(3);

    // Persistently disable Dynamic Resolution in user settings.
    GS->SetDynamicResolutionEnabled(false);
    bChanged = true;

    if (bChanged)
    {
        // Apply both non-res and res settings; confirm and persist.
        GS->ApplyNonResolutionSettings();
        GS->ApplyResolutionSettings(false);
        GS->ConfirmVideoMode();
        GS->SaveSettings();

        UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    }
}

void USimGameInstance::EnforceRuntimeCvars()
{
    // TAA (required for TSR path)
    if (IConsoleVariable* CVarAAMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AntiAliasing")))
    {
        // 0=None, 1=FXAA, 2=TAA, 3=MSAA
        CVarAAMethod->Set(2, ECVF_SetByGameSetting);
    }

    // TSR ON (TemporalAA Upsampling)
    if (IConsoleVariable* CVarUpsample = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAA.Upsampling")))
    {
        CVarUpsample->Set(1, ECVF_SetByGameSetting);
    }

    // Keep primary scale at 100% (present at native; UI crisp)
    if (IConsoleVariable* CVarPrimarySP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
    {
        CVarPrimarySP->Set(100.0f, ECVF_SetByGameSetting);
    }

    // TSR "Quality" internal render scale (~77%)
    if (IConsoleVariable* CVarSecondarySP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport")))
    {
        CVarSecondarySP->Set(77, ECVF_SetByGameSetting);
    }

    // Disable other upscalers (harmless if plugins aren’t present)
    if (IConsoleVariable* CVarDLSS = IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.DLSS.Enable")))
    {
        CVarDLSS->Set(0, ECVF_SetByGameSetting);
    }
    if (IConsoleVariable* CVarFSR3 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FSR3.Enabled")))
    {
        CVarFSR3->Set(0, ECVF_SetByGameSetting);
    }

    // HARD OFF: Dynamic Resolution
    if (IConsoleVariable* CVarDynResMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode")))
    {
        CVarDynResMode->Set(0, ECVF_SetByGameSetting);
    }
    if (IConsoleVariable* CVarDynResMin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.MinScreenPercentage")))
    {
        CVarDynResMin->Set(100.0f, ECVF_SetByGameSetting);
    }
    if (IConsoleVariable* CVarDynResMax = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.MaxScreenPercentage")))
    {
        CVarDynResMax->Set(100.0f, ECVF_SetByGameSetting);
    }

    // Mild sharpen (optional)
    if (IConsoleVariable* CVarSharpen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen")))
    {
        CVarSharpen->Set(0.2f, ECVF_SetByGameSetting);
    }
}

void USimGameInstance::VerifyAndForceOutputResolution()
{
    UGameUserSettings* GS = (GEngine ? GEngine->GetGameUserSettings() : nullptr);
    if (!GS || !GEngine || !GEngine->GameViewport) return;

    const FIntPoint DesktopRes = GS->GetDesktopResolution();

    // Read the actual viewport size (backbuffer)
    FIntPoint VpPx(0, 0);
    if (FViewport* VP = GEngine->GameViewport->Viewport)
    {
        VpPx = VP->GetSizeXY();
    }
    else
    {
        FVector2D Tmp;
        GEngine->GameViewport->GetViewportSize(Tmp);
        VpPx = FIntPoint(FMath::RoundToInt(Tmp.X), FMath::RoundToInt(Tmp.Y));
    }

    const bool bMismatch =
        (DesktopRes.X > 0 && DesktopRes.Y > 0) &&
        (FMath::Abs(VpPx.X - DesktopRes.X) > 1 || FMath::Abs(VpPx.Y - DesktopRes.Y) > 1);

    if (bMismatch)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SimGI] Viewport %dx%d != Desktop %dx%d; forcing WindowedFullscreen at desktop."),
            VpPx.X, VpPx.Y, DesktopRes.X, DesktopRes.Y);

        // 1) Ask the system to change (works well in packaged)
        GS->RequestResolutionChange(DesktopRes.X, DesktopRes.Y, EWindowMode::WindowedFullscreen, false);

        // 2) Apply & confirm for persistence
        GS->SetScreenResolution(DesktopRes);
        GS->SetFullscreenMode(EWindowMode::WindowedFullscreen);
        GS->ApplyResolutionSettings(false);
        GS->ConfirmVideoMode();
        GS->SaveSettings();

        // 3) Belt-and-suspenders: tell the renderer too
        const FString Cmd = FString::Printf(TEXT("r.SetRes %dx%dwf"), DesktopRes.X, DesktopRes.Y);
        UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), Cmd, nullptr);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[SimGI] Viewport matches Desktop: %dx%d."), VpPx.X, VpPx.Y);
    }
}
