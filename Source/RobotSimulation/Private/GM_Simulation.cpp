#include "GM_Simulation.h"
#include "SimulationRobotPawn.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

AGM_Simulation::AGM_Simulation()
{
    PrimaryActorTick.bCanEverTick = false;
    DefaultPawnClass = ASimulationRobotPawn::StaticClass();
}

void AGM_Simulation::BeginPlay()
{
    Super::BeginPlay();

    RobotPawn = Cast<ASimulationRobotPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!IsValid(RobotPawn))
    {
        UE_LOG(LogTemp, Error, TEXT("GM_Simulation: SimulationRobotPawn not found!"));
        LogScreen(TEXT("No SimulationRobotPawn found."), FColor::Red, 5.f);
        return;
    }

    ResolvePlanningViewActor();
    SetupPlanningMode();
}

void AGM_Simulation::ResolvePlanningViewActor()
{
    PlanningViewActor = nullptr;

    if (!PlanningViewTag.IsNone())
    {
        TArray<AActor*> Tagged;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), PlanningViewTag, Tagged);
        for (AActor* A : Tagged)
        {
            if (A && A->IsA(ACameraActor::StaticClass()))
            {
                PlanningViewActor = A;
                break;
            }
        }
    }

    if (!PlanningViewActor)
    {
        TArray<AActor*> Cams;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACameraActor::StaticClass(), Cams);
        if (Cams.Num() > 0)
        {
            PlanningViewActor = Cams[0];
            UE_LOG(LogTemp, Warning, TEXT("Planning camera fallback used: %s"),
                *PlanningViewActor->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No CameraActor found for planning. Will use pawn aerial cam."));
        }
    }
}

void AGM_Simulation::StartSimulation()
{
    if (CurrentState == ESimulationState::Simulating)
    {
        LogScreen(TEXT("Simulation already running."));
        return;
    }

    if (CheckpointLocations.Num() == 0)
    {
        LogScreen(TEXT("Place at least one checkpoint before starting."), FColor::Red, 3.f);
        return;
    }

    CurrentState = ESimulationState::Simulating;
    SetupSimulationMode();
}

void AGM_Simulation::StopSimulation()
{
    if (CurrentState == ESimulationState::Planning)
    {
        LogScreen(TEXT("Already in planning mode."));
        return;
    }

    CurrentState = ESimulationState::Planning;
    SetupPlanningMode();
}

void AGM_Simulation::AddCheckpoint(FVector WorldLocation)
{
    if (CurrentState != ESimulationState::Planning)
    {
        LogScreen(TEXT("Can't add checkpoints while running."), FColor::Red);
        return;
    }
    CheckpointLocations.Add(WorldLocation);
}

void AGM_Simulation::ClearCheckpoints()
{
    if (CurrentState != ESimulationState::Planning) return;
    CheckpointLocations.Empty();
}

void AGM_Simulation::RemoveLastCheckpoint()
{
    if (CurrentState != ESimulationState::Planning) return;
    if (CheckpointLocations.Num() > 0)
        CheckpointLocations.RemoveAt(CheckpointLocations.Num() - 1);
}

void AGM_Simulation::SetupPlanningMode()
{
    if (!IsValid(RobotPawn)) return;

    // robot returns to player & stops patrolling
    if (RobotPawn->IsPatrolling()) RobotPawn->EndMission();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));

        ResolvePlanningViewActor();
        if (PlanningViewActor)
        {
            FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
            PC->SetViewTargetWithBlend(PlanningViewActor, P.BlendTime, P.BlendFunction);
        }
        else
        {
            RobotPawn->SetAerialView(true); // fallback to pawn aerial cam
        }
    }

    // show planning widget (use owning player overload)
    if (!MapOverviewWidget && MapOverviewWidgetClass && PC)
    {
        MapOverviewWidget = CreateWidget<UUserWidget>(PC, MapOverviewWidgetClass);
        if (MapOverviewWidget) MapOverviewWidget->AddToViewport(10);
    }
    if (MapOverviewWidget) MapOverviewWidget->SetVisibility(ESlateVisibility::Visible);

    // hide sim HUDs
    if (RobotStatsWidget) RobotStatsWidget->SetVisibility(ESlateVisibility::Hidden);
    if (PatrolInfoWidget) PatrolInfoWidget->SetVisibility(ESlateVisibility::Hidden);

    UE_LOG(LogTemp, Log, TEXT("Entered planning mode"));
}

void AGM_Simulation::SetupSimulationMode()
{
    // Hardened guards (this is where your crash was reported)
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!IsValid(RobotPawn) || !IsValid(PC))
    {
        UE_LOG(LogTemp, Error, TEXT("SetupSimulationMode: RobotPawn or PlayerController invalid"));
        return;
    }

    // hide planning UI safely
    if (MapOverviewWidget) MapOverviewWidget->SetVisibility(ESlateVisibility::Hidden);

    // create/show sim HUDs (owning player overload prevents null crashes)
    if (!RobotStatsWidget && RobotStatsWidgetClass && PC)
    {
        RobotStatsWidget = CreateWidget<UUserWidget>(PC, RobotStatsWidgetClass);
        if (RobotStatsWidget) RobotStatsWidget->AddToViewport(5);
    }
    if (!PatrolInfoWidget && PatrolInfoWidgetClass && PC)
    {
        PatrolInfoWidget = CreateWidget<UUserWidget>(PC, PatrolInfoWidgetClass);
        if (PatrolInfoWidget) PatrolInfoWidget->AddToViewport(5);
    }
    if (RobotStatsWidget) RobotStatsWidget->SetVisibility(ESlateVisibility::Visible);
    if (PatrolInfoWidget) PatrolInfoWidget->SetVisibility(ESlateVisibility::Visible);

    // apply checkpoints first
    RobotPawn->SetPatrolCheckpoints(CheckpointLocations);

    // blend camera back to robot
    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly());
    FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
    PC->SetViewTargetWithBlend(RobotPawn, P.BlendTime, P.BlendFunction);

    // delay AI start slightly so we don't race the camera transition
    FTimerHandle Th;
    GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (IsValid(RobotPawn)) RobotPawn->BeginMission();
        }), 0.05f, false);

    UE_LOG(LogTemp, Log, TEXT("Entered simulation mode"));
}

void AGM_Simulation::LogScreen(const FString& Msg, FColor Col, float Time) const
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, Time, Col, Msg);
}
