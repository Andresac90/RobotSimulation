#include "GM_Simulation.h"
#include "SimulationRobotPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

AGM_Simulation::AGM_Simulation()
{
    PrimaryActorTick.bCanEverTick = false;

    // Spawn the robot pawn as the default player pawn
    DefaultPawnClass = ASimulationRobotPawn::StaticClass();
}

void AGM_Simulation::BeginPlay()
{
    Super::BeginPlay();

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
    if (!RobotPawn) return;

    // Ensure robot is not patrolling and give the player the pawn back
    if (RobotPawn->IsPatrolling())
    {
        RobotPawn->EndMission();
    }

    // Switch to aerial camera while PC still possesses the pawn
    RobotPawn->SetAerialView(true);

    // Show map overview widget
    if (MapOverviewWidgetClass && !MapOverviewWidget)
    {
        MapOverviewWidget = CreateWidget<UUserWidget>(GetWorld(), MapOverviewWidgetClass);
        if (MapOverviewWidget) MapOverviewWidget->AddToViewport(10);
    }
    else if (MapOverviewWidget)
    {
        MapOverviewWidget->SetVisibility(ESlateVisibility::Visible);
    }

    // Hide simulation HUDs if present
    if (RobotStatsWidget) RobotStatsWidget->SetVisibility(ESlateVisibility::Hidden);
    if (PatrolInfoWidget) PatrolInfoWidget->SetVisibility(ESlateVisibility::Hidden);

    // Enable mouse cursor and UI input
    if (auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    }

    UE_LOG(LogTemp, Log, TEXT("Entered planning mode"));
}

void AGM_Simulation::SetupSimulationMode()
{
    if (!RobotPawn) return;

    // Hide map overview widget
    if (MapOverviewWidget) MapOverviewWidget->SetVisibility(ESlateVisibility::Hidden);

    // Smoothly blend to third-person camera while still possessed by the player
    RobotPawn->SetAerialView(false);

    // Set up checkpoints for patrol and start the mission (AI takes over)
    RobotPawn->SetPatrolCheckpoints(CheckpointLocations);
    RobotPawn->BeginMission();

    // Bring up simulation HUDs (create on demand)
    if (!RobotStatsWidget && RobotStatsWidgetClass)
    {
        RobotStatsWidget = CreateWidget<UUserWidget>(GetWorld(), RobotStatsWidgetClass);
        if (RobotStatsWidget) RobotStatsWidget->AddToViewport(5);
    }
    if (!PatrolInfoWidget && PatrolInfoWidgetClass)
    {
        PatrolInfoWidget = CreateWidget<UUserWidget>(GetWorld(), PatrolInfoWidgetClass);
        if (PatrolInfoWidget) PatrolInfoWidget->AddToViewport(5);
    }
    if (RobotStatsWidget) RobotStatsWidget->SetVisibility(ESlateVisibility::Visible);
    if (PatrolInfoWidget) PatrolInfoWidget->SetVisibility(ESlateVisibility::Visible);

    // Gameplay input (mouse not needed now)
    if (auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    UE_LOG(LogTemp, Log, TEXT("Entered simulation mode"));
}
