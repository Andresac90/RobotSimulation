#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "PatrolVehicleMovementComponent.h"
#include "RobotAIController.h"
#include "Navigation/PathFollowingComponent.h" // for FPathFollowingResult
#include "AITypes.h"                           // for FAIRequestID
#include "Blueprint/UserWidget.h"
#include "Waypoint.h"
#include "SimulationRobotPawn.generated.h"

/**
 * A wheeled “robot” pawn that can toggle between manual driving and AI patrol mode.
 */
UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);

    /** Are we currently in AI‑patrol mode? */
    UPROPERTY(BlueprintReadWrite, Category = "Stats")
    bool bIsPatrolMode = false;

    /** Last manual throttle input value */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastThrottleVal = 0.f;

    /** Last manual steering input value */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastSteeringVal = 0.f;

    /** Whether the engine max‑RPM is currently limited */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    bool bSpeedLimited = true;

    /** Index of the current waypoint in Waypoints[] */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 CurrentWPIndex = 0;

protected:
    // Standard pawn lifecycle
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Manual‑drive handlers
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);

    /** Bound to input: toggles AI patrol on/off */
    UFUNCTION(BlueprintCallable, Category = "Patrol")
    void TogglePatrolMode();

    /** Bound to input: toggles engine speed limit on/off */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void ToggleSpeedLimit();

    /** How close to a waypoint before we consider it “reached” */
    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 200.f;

    /** Optional HUD widget class to display stats/UI */
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

private:
    // All waypoints found in the level, sorted by PatrolOrder
    TArray<AWaypoint*> Waypoints;

    // The AI controller that will actually drive us
    AAIController* AICon = nullptr;

    // Called whenever a MoveToActor request finishes
    void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

    // Applies the current speed‑limit setting to the vehicle’s engine
    void ApplySpeedLimit();

    // Spawned HUD instance (if any)
    UUserWidget* HUDWidget = nullptr;
};
