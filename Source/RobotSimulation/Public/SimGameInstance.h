#pragma once
#include "Engine/GameInstance.h"
#include "SimGameInstance.generated.h"

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
