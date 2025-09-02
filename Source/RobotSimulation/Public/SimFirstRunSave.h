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

    // Old flag (kept): we ran the hardware benchmark + applied scalability
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FirstRun")
    bool bGraphicsBenchmarked;

    // NEW: we set native desktop resolution + borderless fullscreen
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FirstRun")
    bool bResolutionConfigured;

    // NEW: we enabled TSR upscaling + set screen percentage
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FirstRun")
    bool bTSRConfigured;
};
