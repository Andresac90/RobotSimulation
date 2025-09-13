#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SimGameInstance.generated.h"

/**
 * One-time graphics setup on first launch:
 * • Borderless fullscreen at desktop native resolution (keeps device aspect ratio)
 * • Resolution scale = 100% (native pixels)
 * • Enable TSR and set TSR Quality preset
 * • Auto-detect hardware and apply recommended scalability
 * • Force max view distance for Landscapes
 */
UCLASS()
class ROBOTSIMULATION_API USimGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

private:
    void RunFirstLaunchGraphicsSetupIfNeeded();

    UPROPERTY() FString FirstRunSlot = TEXT("FirstRun");
};
