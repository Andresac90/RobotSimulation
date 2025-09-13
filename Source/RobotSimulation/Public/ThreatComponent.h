#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThreatComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROBOTSIMULATION_API UThreatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat")
    FName ThreatLabel = "Threat";
};
