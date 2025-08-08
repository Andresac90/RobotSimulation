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

    // The live pawn instance (kept up to date by BeginPlay/EndPlay + resolver)
    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    ASimulationRobotPawn* RobotPawn = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Simulation")
    TArray<FVector> CheckpointLocations;

    // Widgets (set these in the GM Blueprint)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> MapOverviewWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> RobotStatsWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> PatrolInfoWidgetClass;

    // Tag of a CameraActor used in planning view. Optional.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FName PlanningViewTag = TEXT("PlanningView");

    // Set to BP_RobotPawn in your GM blueprint (Class Defaults → Simulation → Robot Pawn Class)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
    TSubclassOf<ASimulationRobotPawn> RobotPawnClass;

    // --- Commands exposed to UI / Blueprints ---
    UFUNCTION(BlueprintCallable, Category = "Simulation") void StartSimulation();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void StopSimulation();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void AddCheckpoint(FVector WorldLocation);
    UFUNCTION(BlueprintCallable, Category = "Simulation") void ClearCheckpoints();
    UFUNCTION(BlueprintCallable, Category = "Simulation") void RemoveLastCheckpoint();

    UFUNCTION(BlueprintPure, Category = "Simulation")
    TArray<FVector> GetCheckpoints() const { return CheckpointLocations; }

    // So widgets don’t depend on “Owning Player Pawn” (which becomes AI-owned)
    UFUNCTION(BlueprintPure, Category = "Simulation")
    ASimulationRobotPawn* GetRobotPawn() const { return RobotPawn; }

    // Handy onscreen log (make it public so pawns can call it)
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void LogScreen(const FString& Msg, FColor Col = FColor::Yellow, float Time = 2.f) const;

    // Pawn calls this in BeginPlay/EndPlay to keep pointer fresh
    UFUNCTION() void NotifyRobotReady(ASimulationRobotPawn* P) { RobotPawn = P; }

private:
    void SetupPlanningMode();
    void SetupSimulationMode();
    void ResolvePlanningViewActor();
    void ReassertRobotView(float DelaySeconds);

    // Finds existing robot pawn or spawns one using RobotPawnClass
    ASimulationRobotPawn* ResolveOrSpawnRobotPawn();

    // widget instances
    UPROPERTY() UUserWidget* MapOverviewWidget = nullptr;
    UPROPERTY() UUserWidget* RobotStatsWidget = nullptr;
    UPROPERTY() UUserWidget* PatrolInfoWidget = nullptr;

    // planning camera actor
    UPROPERTY() AActor* PlanningViewActor = nullptr;
};
