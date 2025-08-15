// .h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class ROBOTSIMULATION_API ACheckpointMarker : public AActor
{
    GENERATED_BODY()
public:
    ACheckpointMarker();

    UPROPERTY(VisibleAnywhere) USceneComponent* Root;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere) UTextRenderComponent* Label;

    UFUNCTION() void InitMarker(int32 Index);
};
