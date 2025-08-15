#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GM_Simulation.generated.h"

class ASimulationRobotPawn;
class UUserWidget;
class ACheckpointMarker;

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
    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ESimulationState CurrentState = ESimulationState::Planning;

    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ASimulationRobotPawn* RobotPawn = nullptr;   // live instance

    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    TArray<FVector> CheckpointLocations;

    // UI classes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> MapOverviewWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> RobotStatsWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> PatrolInfoWidgetClass;

    // Planning camera (optional)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FName PlanningViewTag = TEXT("PlanningView");

    // Set this to BP_RobotPawn in your BP GameMode
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
    TSubclassOf<ASimulationRobotPawn> RobotPawnClass;

    // Optional visual waypoint actor to spawn in planning
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Markers")
    TSubclassOf<ACheckpointMarker> CheckpointMarkerClass;

    // Commands the UI calls
    UFUNCTION(BlueprintCallable, Category = "Simulation") void StartSimulation();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void StopSimulation();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void AddCheckpoint(FVector WorldLocation);
    UFUNCTION(BlueprintCallable, Category = "Simulation") void ClearCheckpoints();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void RemoveLastCheckpoint();

    UFUNCTION(BlueprintPure, Category = "Simulation")
    TArray<FVector> GetCheckpoints() const { return CheckpointLocations; }

    // Widgets use this (don’t rely on Owning Player Pawn)
    UFUNCTION(BlueprintPure, Category = "Simulation")
    ASimulationRobotPawn* GetRobotPawn() const { return RobotPawn; }

    // Pawn notifies GM in BeginPlay/EndPlay
    UFUNCTION() void NotifyRobotReady(ASimulationRobotPawn* P) { RobotPawn = P; }

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void LogScreen(const FString& Msg, FColor Col = FColor::Yellow, float Time = 2.f) const;

private:
    void SetupPlanningMode();
    void SetupSimulationMode();
    void ResolvePlanningViewActor();
    void ReassertRobotView();
    ASimulationRobotPawn* ResolveOrSpawnRobotPawn();

    // widgets
    UPROPERTY() UUserWidget* MapOverviewWidget = nullptr;
    UPROPERTY() UUserWidget* RobotStatsWidget = nullptr;
    UPROPERTY() UUserWidget* PatrolInfoWidget = nullptr;

    // planning cam
    UPROPERTY() AActor* PlanningViewActor = nullptr;

    // spawned visual markers
    UPROPERTY() TArray<TWeakObjectPtr<AActor>> CheckpointMarkers;
    void RefreshMarkers();
    void UpdateMarkerIndices();
};
