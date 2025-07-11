#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"  // FPathFollowingResult
#include "AITypes.h"                            // FAIRequestID
#include "Blueprint/UserWidget.h"
#include "Waypoint.h"                           // our C++ waypoint class
#include "SimulationRobotPawn.generated.h"

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);

    UPROPERTY(BlueprintReadWrite, Category = "Stats")
    bool bIsPatrolMode = false;

    // UI stats
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastThrottleVal = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastSteeringVal = 0.f;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Manual controls
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);

    // Toggle patrol on/off (optional)
    UFUNCTION(BlueprintCallable, Category = "Patrol")
    void TogglePatrolMode();

    // How close before we switch to the next waypoint
    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 200.f;

    // HUD widget
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

private:
    // Ordered list of patrol points
    TArray<AWaypoint*> Waypoints;

    // Which waypoint index we’re heading to now
    int32 CurrentWPIndex = 0;

    // Cached AIController
    AAIController* AICon = nullptr;

    // Called when a MoveToActor completes
    void OnMoveCompleted(FAIRequestID, const FPathFollowingResult&);

    // Spawned HUD
    UUserWidget* HUDWidget = nullptr;
};
