#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SimGameInstance.generated.h"

UCLASS()
class ROBOTSIMULATION_API USimGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

private:
    // One-time on first launch: benchmark, lock res scale, disable dyn-res.
    void RunFirstLaunchGraphicsSetupIfNeeded();

    // Every startup & every map init: TSR Quality + crisp UI + dyn-res OFF.
    void EnforceRuntimeCvars();

    // Match desktop/native res + borderless fullscreen.
    void ApplyDesktopResolution();

    // Re-assert after each world init (editor & packaged).
    void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

    UPROPERTY() FString FirstRunSlot = TEXT("FirstRun");
};
