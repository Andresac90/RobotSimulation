// SimulationRobotPawn.h
#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"                   // ChaosVehiclesPlugin
#include "ChaosWheeledVehicleMovementComponent.h" // the 4-wheel movement comp
#include "SimulationRobotPawn.generated.h"

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);

protected:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Manual input
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

private:
    int32 CurrentWPIndex = 0;
    bool  bPatrolMode = false;
};
