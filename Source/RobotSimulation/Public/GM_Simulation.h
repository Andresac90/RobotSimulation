#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GM_Simulation.generated.h"

class ASimulationRobotPawn;
class UUserWidget;

UENUM(BlueprintType)
enum class ESimulationState : uint8
{
    Planning,
    Simulating,
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

    // —— UI classes (assign in World Settings → GameMode Override) ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> MapOverviewWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> RobotStatsWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> PatrolInfoWidgetClass;

    // —— Option C: CameraActor in the level with this tag will be used for planning view ——
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FName PlanningViewTag = TEXT("PlanningView");

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
    void ResolvePlanningViewActor();

    // widget instances
    UPROPERTY() UUserWidget* MapOverviewWidget = nullptr;
    UPROPERTY() UUserWidget* RobotStatsWidget = nullptr;
    UPROPERTY() UUserWidget* PatrolInfoWidget = nullptr;

    // resolved planning camera at runtime (CameraActor with PlanningViewTag)
    UPROPERTY() AActor* PlanningViewActor = nullptr;

    void LogScreen(const FString& Msg, FColor Col = FColor::Yellow, float Time = 2.f) const;
};
