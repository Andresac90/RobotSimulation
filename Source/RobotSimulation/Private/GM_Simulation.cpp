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
    DefaultPawnClass = ASimulationRobotPawn::StaticClass(); // your BP GameMode can override this
}

void AGM_Simulation::BeginPlay()
{
    Super::BeginPlay();

    RobotPawn = Cast<ASimulationRobotPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
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
            UE_LOG(LogTemp, Warning, TEXT("Planning camera fallback used: %s"), *PlanningViewActor->GetName());
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
    if (RobotPawn && RobotPawn->IsPatrolling())
        RobotPawn->EndMission();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC->bAutoManageActiveCameraTarget = true; // enable automatic cam in planning
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));

        ResolvePlanningViewActor();
        if (PlanningViewActor)
        {
            FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
            PC->SetViewTargetWithBlend(PlanningViewActor, P.BlendTime, P.BlendFunction);
        }
        else if (RobotPawn)
        {
            RobotPawn->SetAerialView(true);
        }
    }

    if (!MapOverviewWidget && MapOverviewWidgetClass && PC)
    {
        MapOverviewWidget = CreateWidget<UUserWidget>(PC, MapOverviewWidgetClass);
        if (MapOverviewWidget) MapOverviewWidget->AddToViewport(10);
    }
    if (MapOverviewWidget) MapOverviewWidget->SetVisibility(ESlateVisibility::Visible);
    if (RobotStatsWidget)  RobotStatsWidget->SetVisibility(ESlateVisibility::Hidden);
    if (PatrolInfoWidget)  PatrolInfoWidget->SetVisibility(ESlateVisibility::Hidden);
}

void AGM_Simulation::SetupSimulationMode()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("SetupSimulationMode: PlayerController invalid"));
        return;
    }

    // Re-resolve in case BP default pawn spawned now
    RobotPawn = Cast<ASimulationRobotPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!RobotPawn)
    {
        TArray<AActor*> All;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASimulationRobotPawn::StaticClass(), All);
        if (All.Num() > 0) RobotPawn = Cast<ASimulationRobotPawn>(All[0]);
    }
    if (!RobotPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("SetupSimulationMode: Could not resolve ASimulationRobotPawn"));
        LogScreen(TEXT("Robot pawn missing."), FColor::Red, 3.f);
        return;
    }

    if (MapOverviewWidget) MapOverviewWidget->SetVisibility(ESlateVisibility::Hidden);
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

    // Push checkpoints & build a dedicated follow camera actor on the pawn
    RobotPawn->SetPatrolCheckpoints(CheckpointLocations);
    RobotPawn->ForceThirdPersonCamera();
    AActor* ViewTargetActor = RobotPawn->GetThirdPersonViewTarget();

    // Don't let PC auto-switch to spectator after unpossess
    PC->bAutoManageActiveCameraTarget = false;

    // Blend to the dedicated camera actor
    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly());
    FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
    if (ViewTargetActor)
    {
        PC->SetViewTargetWithBlend(ViewTargetActor, P.BlendTime, P.BlendFunction);
    }
    else
    {
        // Fallback to pawn (CalcCamera handles it)
        PC->SetViewTargetWithBlend(RobotPawn, P.BlendTime, P.BlendFunction);
    }

    // Start AI after a tiny delay
    FTimerHandle ThStart;
    GetWorldTimerManager().SetTimer(ThStart, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (RobotPawn) RobotPawn->BeginMission();
        }), 0.05f, false);

    // Re-assert view shortly after AI possession in case anything shuffled it
    FTimerHandle ThReassert;
    GetWorldTimerManager().SetTimer(ThReassert, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (APlayerController* PC2 = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (RobotPawn)
                {
                    if (AActor* Again = RobotPawn->GetThirdPersonViewTarget())
                        PC2->SetViewTarget(Again);
                    else
                        PC2->SetViewTarget(RobotPawn);
                }
            }
        }), 0.15f, false);
}

void AGM_Simulation::LogScreen(const FString& Msg, FColor Col, float Time) const
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, Time, Col, Msg);
}
