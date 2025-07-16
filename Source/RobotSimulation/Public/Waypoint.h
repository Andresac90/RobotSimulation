#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"
#include "Waypoint.generated.h"

UCLASS()
class ROBOTSIMULATION_API AWaypoint : public AActor
{
    GENERATED_BODY()

public:
    AWaypoint();

    /** Sequence index for the patrol */
    UPROPERTY(EditInstanceOnly, Category = "Patrol")
    int32 PatrolOrder = 0;

#if WITH_EDITORONLY_DATA
    /** Simple icon so you can see this in the viewport */
    UPROPERTY()
    UBillboardComponent* SpriteComponent;
#endif
};
