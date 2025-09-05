// SimFirstRunSave.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SimFirstRunSave.generated.h"

UCLASS(BlueprintType)
class ROBOTSIMULATION_API USimFirstRunSave : public USaveGame
{
    GENERATED_BODY()

public:
    USimFirstRunSave();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FirstRun")
    bool bGraphicsBenchmarked;
};
