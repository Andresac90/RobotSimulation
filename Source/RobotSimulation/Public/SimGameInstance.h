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
    // First-run: run hardware benchmark, set desktop/native, borderless, lock primary scale, disable dynamic res.
    void RunFirstLaunchGraphicsSetupIfNeeded();

    // Enforce TSR Quality + crisp output + DR off each time.
    void EnforceRuntimeCvars();

    // Called when a world finishes initialization (editor & packaged).
    void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

    // Verify actual backbuffer and force native output if it didn’t stick.
    void VerifyAndForceOutputResolution();

    // Also verify on the first tick after Init (extra safety).
    bool TickerVerify(float DeltaSeconds);

    UPROPERTY() FString FirstRunSlot = TEXT("FirstRun");
};
