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
    // First-run: force Epic scalability, borderless-native, resolution scale 70%, enable TSR.
    void RunFirstLaunchGraphicsSetupIfNeeded();

    UPROPERTY() FString FirstRunSlot = TEXT("FirstRun");
};
