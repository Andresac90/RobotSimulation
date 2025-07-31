#include "GM_Simulation.h"
#include "SimulationRobotPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

AGM_Simulation::AGM_Simulation()
{
    PrimaryActorTick.bCanEverTick = false;

    // Set default pawn class to our robot
    DefaultPawnClass = ASimulationRobotPawn::StaticClass();
}

void AGM_Simulation::BeginPlay()
{
    Super::BeginPlay();

    // Find the robot pawn
    RobotPawn = Cast<ASimulationRobotPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

    if (!RobotPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("GM_Simulation: Could not find SimulationRobotPawn!"));
        return;
    }

    // Start in planning mode
    SetupPlanningMode();
}

void AGM_Simulation::StartSimulation()
{
    if (CurrentState == ESimulationState::Simulating)
    {
        UE_LOG(LogTemp, Warning, TEXT("Simulation already running!"));
        return;
    }

    if (CheckpointLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No checkpoints set! Cannot start simulation."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Starting simulation with %d checkpoints"), CheckpointLocations.Num());

    CurrentState = ESimulationState::Simulating;
    SetupSimulationMode();
}

void AGM_Simulation::StopSimulation()
{
    if (CurrentState == ESimulationState::Planning)
    {
        UE_LOG(LogTemp, Warning, TEXT("Already in planning mode!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Stopping simulation, returning to planning mode"));

    CurrentState = ESimulationState::Planning;
    SetupPlanningMode();
}

void AGM_Simulation::AddCheckpoint(FVector WorldLocation)
{
    if (CurrentState != ESimulationState::Planning)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot add checkpoints while simulation is running!"));
        return;
    }

    CheckpointLocations.Add(WorldLocation);
    UE_LOG(LogTemp, Log, TEXT("Added checkpoint at: %s (Total: %d)"),
        *WorldLocation.ToString(), CheckpointLocations.Num());
}

void AGM_Simulation::ClearCheckpoints()
{
    if (CurrentState != ESimulationState::Planning)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot clear checkpoints while simulation is running!"));
        return;
    }

    CheckpointLocations.Empty();
    UE_LOG(LogTemp, Log, TEXT("Cleared all checkpoints"));
}

void AGM_Simulation::RemoveLastCheckpoint()
{
    if (CurrentState != ESimulationState::Planning)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot remove checkpoints while simulation is running!"));
        return;
    }

    if (CheckpointLocations.Num() > 0)
    {
        CheckpointLocations.RemoveAt(CheckpointLocations.Num() - 1);
        UE_LOG(LogTemp, Log, TEXT("Removed last checkpoint (Remaining: %d)"), CheckpointLocations.Num());
    }
}

void AGM_Simulation::SetupPlanningMode()
{
    if (!RobotPawn)
        return;

    // Make sure robot is not in patrol mode
    if (RobotPawn->IsPatrolling())
    {
        RobotPawn->EndMission();
    }

    // Switch to aerial camera
    RobotPawn->SetAerialView(true);

    // Show map overview widget
    if (MapOverviewWidgetClass && !MapOverviewWidget)
    {
        MapOverviewWidget = CreateWidget<UUserWidget>(GetWorld(), MapOverviewWidgetClass);
        if (MapOverviewWidget)
        {
            MapOverviewWidget->AddToViewport();
        }
    }
    else if (MapOverviewWidget)
    {
        MapOverviewWidget->SetVisibility(ESlateVisibility::Visible);
    }

    // Enable mouse cursor and UI input
    if (auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI()
            .SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    }

    UE_LOG(LogTemp, Log, TEXT("Entered planning mode"));
}

void AGM_Simulation::SetupSimulationMode()
{
    if (!RobotPawn)
        return;

    // Hide map overview widget
    if (MapOverviewWidget)
    {
        MapOverviewWidget->SetVisibility(ESlateVisibility::Hidden);
    }

    // Switch to third person camera
    RobotPawn->SetAerialView(false);

    // Set up checkpoints for patrol
    RobotPawn->SetPatrolCheckpoints(CheckpointLocations);

    // Start the mission
    RobotPawn->BeginMission();

    // Set input mode for gameplay
    if (auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    UE_LOG(LogTemp, Log, TEXT("Entered simulation mode"));
}