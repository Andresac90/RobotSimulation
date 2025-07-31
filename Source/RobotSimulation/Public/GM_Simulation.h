#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "GM_Simulation.generated.h"

class ASimulationRobotPawn;
class UUserWidget;

UENUM(BlueprintType)
enum class ESimulationState : uint8
{
    Planning,    // Aerial view, placing checkpoints
    Simulating,  // Robot view, following patrol
    Paused
};

UCLASS()
class ROBOTSIMULATION_API AGM_Simulation : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGM_Simulation();

protected:
    virtual void BeginPlay() override;

public:
    /** Current simulation state */
    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ESimulationState CurrentState = ESimulationState::Planning;

    /** Reference to the robot pawn */
    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ASimulationRobotPawn* RobotPawn = nullptr;

    /** Array of checkpoint locations set by the player */
    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    TArray<FVector> CheckpointLocations;

    /** Map overview widget class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> MapOverviewWidgetClass;

    /** Current map overview widget instance */
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* MapOverviewWidget = nullptr;

    /** Start the simulation with current checkpoints */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void StartSimulation();

    /** Stop simulation and return to planning mode */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void StopSimulation();

    /** Add a checkpoint at the specified world location */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void AddCheckpoint(FVector WorldLocation);

    /** Clear all checkpoints */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void ClearCheckpoints();

    /** Remove the last checkpoint */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void RemoveLastCheckpoint();

    /** Get the current checkpoints for UI display */
    UFUNCTION(BlueprintPure, Category = "Simulation")
    TArray<FVector> GetCheckpoints() const { return CheckpointLocations; }

private:
    void SetupPlanningMode();
    void SetupSimulationMode();
};