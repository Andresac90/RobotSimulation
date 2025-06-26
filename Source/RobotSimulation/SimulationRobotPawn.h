#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Blueprint/UserWidget.h"            // for UUserWidget
#include "SimulationRobotPawn.generated.h"

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Manual driving handlers
    void ThrottleInput(float Val);
    void SteeringInput(float Val);

    // Patrol
    UFUNCTION(BlueprintCallable, Category = "Patrol")
    void TogglePatrolMode();
    void PatrolTick(float DeltaTime);

    // Waypoints
    UPROPERTY(EditAnywhere, Category = "Patrol")
    TArray<AActor*> Waypoints;
    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 200.f;

    // —— UI Stats ——
    // Exposed to Blueprint so UMG can bind to them
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    bool bIsPatrolMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastThrottleVal = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastSteeringVal = 0.f;

    // Widget class (set this to your UMG Blueprint in the editor)
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

private:
    int32 CurrentWPIndex = 0;
    bool  bPatrolMode = false;

    // Instance of the widget
    UUserWidget* HUDWidget = nullptr;
};
