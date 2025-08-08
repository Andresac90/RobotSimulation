#include "GM_Simulation.h"
#include "SimulationRobotPawn.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Camera/CameraActor.h"
#include "NavigationSystem.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

AGM_Simulation::AGM_Simulation()
{
    PrimaryActorTick.bCanEverTick = false;

    // Safe defaults; the BP GameMode can override RobotPawnClass with BP_RobotPawn
    DefaultPawnClass = ASimulationRobotPawn::StaticClass();
    RobotPawnClass = ASimulationRobotPawn::StaticClass();
}

void AGM_Simulation::BeginPlay()
{
    Super::BeginPlay();

    // Make sure there is a pawn and we have a pointer to it
    RobotPawn = ResolveOrSpawnRobotPawn();

    ResolvePlanningViewActor();
    SetupPlanningMode();
}

// -------------------------------------------------------------

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
            UE_LOG(LogTemp, Warning, TEXT("[GM] Planning camera fallback: %s"), *PlanningViewActor->GetName());
        }
    }
}

// Finds or spawns the robot pawn and returns it.
ASimulationRobotPawn* AGM_Simulation::ResolveOrSpawnRobotPawn()
{
    UWorld* W = GetWorld();
    if (!W) return nullptr;

    // 0) Using the already-registered pointer?
    if (RobotPawn && IsValid(RobotPawn))
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] Using registered robot pawn: %s"), *RobotPawn->GetName());
        return RobotPawn;
    }

    // 1) Is the PlayerController already possessing it?
    if (ASimulationRobotPawn* PP = Cast<ASimulationRobotPawn>(UGameplayStatics::GetPlayerPawn(W, 0)))
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] Found player pawn: %s"), *PP->GetName());
        RobotPawn = PP;
        return PP;
    }

    // 2) Any ASimulationRobotPawn actors in the world?
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(W, ASimulationRobotPawn::StaticClass(), Found);
        if (Found.Num() > 0)
        {
            RobotPawn = Cast<ASimulationRobotPawn>(Found[0]);
            UE_LOG(LogTemp, Log, TEXT("[GM] Using existing robot pawn: %s"), *RobotPawn->GetName());
            return RobotPawn;
        }
    }

    // 3) Spawn one (use RobotPawnClass set in BP if provided)
    TSubclassOf<ASimulationRobotPawn> SpawnClass = RobotPawnClass;
    if (!*SpawnClass)
    {
        if (DefaultPawnClass && DefaultPawnClass->IsChildOf(ASimulationRobotPawn::StaticClass()))
            SpawnClass = TSubclassOf<ASimulationRobotPawn>(DefaultPawnClass);
        else
            SpawnClass = ASimulationRobotPawn::StaticClass();
    }

    // Choose a PlayerStart for transform (or identity if none)
    FTransform SpawnTM;
    {
        TArray<AActor*> Starts;
        UGameplayStatics::GetAllActorsOfClass(W, APlayerStart::StaticClass(), Starts);
        if (Starts.Num() > 0) SpawnTM = Starts[0]->GetActorTransform();
        UE_LOG(LogTemp, Log, TEXT("[GM] Spawning robot at %s"), *SpawnTM.GetLocation().ToString());
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASimulationRobotPawn* NewPawn = W->SpawnActor<ASimulationRobotPawn>(SpawnClass, SpawnTM, Params);

    if (NewPawn)
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] Spawned robot pawn: %s (class: %s)"),
            *NewPawn->GetName(), *NewPawn->GetClass()->GetName());
        RobotPawn = NewPawn;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] FAILED to spawn robot pawn (class: %s)"),
            SpawnClass ? *SpawnClass->GetName() : TEXT("<null>"));
    }

    return RobotPawn;
}

// -------------------------------------------------------------

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

// -------------------------------------------------------------

void AGM_Simulation::AddCheckpoint(FVector WorldLocation)
{
    if (CurrentState != ESimulationState::Planning)
    {
        LogScreen(TEXT("Can't add checkpoints while running."), FColor::Red);
        return;
    }

    // Project clicks to the navmesh (keeps vehicle on-road)
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        FNavLocation Proj;
        if (Nav->ProjectPointToNavigation(WorldLocation, Proj, FVector(600, 600, 800)))
            WorldLocation = Proj.Location;
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

// -------------------------------------------------------------

void AGM_Simulation::SetupPlanningMode()
{
    RobotPawn = ResolveOrSpawnRobotPawn();

    if (RobotPawn && RobotPawn->IsPatrolling())
        RobotPawn->EndMission();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        // Keep cursor/UI functional while planning
        PC->bAutoManageActiveCameraTarget = true;
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;

        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);

        ResolvePlanningViewActor();
        if (PlanningViewActor)
        {
            FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
            PC->SetViewTargetWithBlend(PlanningViewActor, P.BlendTime, P.BlendFunction);
        }
        else if (RobotPawn)
        {
            RobotPawn->SetAerialView(true);
            PC->SetViewTarget(RobotPawn);
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
        UE_LOG(LogTemp, Error, TEXT("[GM] SetupSimulationMode: PlayerController invalid"));
        return;
    }

    RobotPawn = ResolveOrSpawnRobotPawn();
    if (!RobotPawn || !IsValid(RobotPawn))
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] SetupSimulationMode: Could not resolve/spawn ASimulationRobotPawn"));
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

    // Feed checkpoints and make sure 3P cam is active on the pawn
    RobotPawn->SetPatrolCheckpoints(CheckpointLocations);
    RobotPawn->ForceThirdPersonCamera();

    // Keep UI clickable *during* simulation
    PC->bAutoManageActiveCameraTarget = false;
    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    PC->bEnableMouseOverEvents = true;

    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(Mode);

    // View the pawn’s stable 3P camera proxy (never invalid, attached to spring arm)
    if (AActor* VT = RobotPawn->GetThirdPersonViewTarget())
    {
        FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
        PC->SetViewTargetWithBlend(VT, P.BlendTime, P.BlendFunction);
    }

    // Kick patrol slightly after blend to avoid races
    FTimerHandle ThStart;
    GetWorldTimerManager().SetTimer(ThStart, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (RobotPawn) RobotPawn->BeginMission();
        }), 0.05f, false);

    // Re-assert view next tick just in case anything shifted
    ReassertRobotView(0.15f);
}

void AGM_Simulation::ReassertRobotView(float /*DelaySeconds*/)
{
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (RobotPawn)
                {
                    if (AActor* Cam = RobotPawn->GetThirdPersonViewTarget())
                        PC->SetViewTarget(Cam);
                    else
                        PC->SetViewTarget(RobotPawn);
                }
            }
        }));
}

void AGM_Simulation::LogScreen(const FString& Msg, FColor Col, float Time) const
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, Time, Col, Msg);
}
