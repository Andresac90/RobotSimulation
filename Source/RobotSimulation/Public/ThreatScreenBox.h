#pragma once
#include "CoreMinimal.h"
#include "ThreatScreenBox.generated.h"

/**
 * Min/Max are viewport-relative pixel coordinates
 * (0,0) = top-left of the player viewport.
 */
USTRUCT(BlueprintType)
struct FThreatScreenBox
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FVector2D Min = FVector2D::ZeroVector; // top-left in viewport pixels
    UPROPERTY(BlueprintReadWrite) FVector2D Max = FVector2D::ZeroVector; // bottom-right in viewport pixels
    UPROPERTY(BlueprintReadWrite) FText     Label;
};
