#pragma once
#include "CoreMinimal.h"
#include "ThreatScreenBox.generated.h"

USTRUCT(BlueprintType)
struct FThreatScreenBox
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FVector2D Min = FVector2D::ZeroVector; // top-left (viewport pixels)
    UPROPERTY(BlueprintReadWrite) FVector2D Max = FVector2D::ZeroVector; // bottom-right
    UPROPERTY(BlueprintReadWrite) FText     Label;
};
