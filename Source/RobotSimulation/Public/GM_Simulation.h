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
    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ESimulationState CurrentState = ESimulationState::Planning;

    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ASimulationRobotPawn* RobotPawn = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    TArray<FVector> CheckpointLocations;

    // UI classes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> MapOverviewWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> RobotStatsWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> PatrolInfoWidgetClass;

    // CameraActor in the level with this tag will be used for planning view
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FName PlanningViewTag = TEXT("PlanningView");

    UFUNCTION(BlueprintCallable, Category = "Simulation") void StartSimulation();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void StopSimulation();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void AddCheckpoint(FVector WorldLocation);
    UFUNCTION(BlueprintCallable, Category = "Simulation") void ClearCheckpoints();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void RemoveLastCheckpoint();
    UFUNCTION(BlueprintPure, Category = "Simulation") TArray<FVector> GetCheckpoints() const { return CheckpointLocations; }

private:
    void SetupPlanningMode();
    void SetupSimulationMode();
    void ResolvePlanningViewActor();

    UPROPERTY() UUserWidget* MapOverviewWidget = nullptr;
    UPROPERTY() UUserWidget* RobotStatsWidget = nullptr;
    UPROPERTY() UUserWidget* PatrolInfoWidget = nullptr;

    UPROPERTY() AActor* PlanningViewActor = nullptr;

    void LogScreen(const FString& Msg, FColor Col = FColor::Yellow, float Time = 2.f) const;
};
