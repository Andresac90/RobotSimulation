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

    /** True once we've applied our one-time graphics setup. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FirstRun")
    bool bFirstRunApplied;
};
