#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"   // for FPathFollowingResult
#include "AITypes.h"                             // for FAIRequestID
#include "Blueprint/UserWidget.h"
#include "SimulationRobotPawn.generated.h"

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Manual driving
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);

    // Patrol toggle
    UFUNCTION(BlueprintCallable, Category = "Patrol")
    void TogglePatrolMode();

    // UI stats
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    bool bIsPatrolMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastThrottleVal = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float LastSteeringVal = 0.f;

    // Waypoints placed in-level and tagged "Waypoint"
    UPROPERTY(EditAnywhere, Category = "Patrol")
    TArray<AActor*> Waypoints;

    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 200.f;

    // HUD widget
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

private:
    int32 CurrentWPIndex = 0;
    AAIController* AICon = nullptr;

    // ← no UFUNCTION() here!
    void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

    UUserWidget* HUDWidget = nullptr;
};
