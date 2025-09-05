#include "GM_Simulation.h" 
#include "SimulationRobotPawn.h"
#include "CheckpointMarker.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Camera/CameraActor.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

AGM_Simulation::AGM_Simulation()
{
    PrimaryActorTick.bCanEverTick = false;

    // Safe defaults; your BP_GM should point RobotPawnClass to BP_RobotPawn
    DefaultPawnClass = ASimulationRobotPawn::StaticClass();
    RobotPawnClass = ASimulationRobotPawn::StaticClass();
}

void AGM_Simulation::BeginPlay()
{
    Super::BeginPlay();
    RobotPawn = ResolveOrSpawnRobotPawn();
    ResolvePlanningViewActor();
    SetupPlanningMode();
}

// -------------------------------------------------------------
// Planning camera resolve
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

// -------------------------------------------------------------
// Robot resolve / spawn
// -------------------------------------------------------------
ASimulationRobotPawn* AGM_Simulation::ResolveOrSpawnRobotPawn()
{
    UWorld* W = GetWorld();
    if (!W) return nullptr;

    if (RobotPawn && IsValid(RobotPawn))
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] Using registered robot pawn: %s"), *RobotPawn->GetName());
        return RobotPawn;
    }

    if (ASimulationRobotPawn* PP = Cast<ASimulationRobotPawn>(UGameplayStatics::GetPlayerPawn(W, 0)))
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] Found player pawn: %s"), *PP->GetName());
        RobotPawn = PP;
        return PP;
    }

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

    // spawn a new one
    TSubclassOf<ASimulationRobotPawn> SpawnClass = RobotPawnClass;
    if (!*SpawnClass)
    {
        if (DefaultPawnClass && DefaultPawnClass->IsChildOf(ASimulationRobotPawn::StaticClass()))
            SpawnClass = TSubclassOf<ASimulationRobotPawn>(DefaultPawnClass);
        else
            SpawnClass = ASimulationRobotPawn::StaticClass();
    }

    FTransform SpawnTM;
    {
        TArray<AActor*> Starts;
        UGameplayStatics::GetAllActorsOfClass(W, APlayerStart::StaticClass(), Starts);
        if (Starts.Num() > 0) SpawnTM = Starts[0]->GetActorTransform();
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASimulationRobotPawn* NewPawn = W->SpawnActor<ASimulationRobotPawn>(SpawnClass, SpawnTM, Params);

    if (NewPawn)
    {
        UE_LOG(LogTemp, Log, TEXT("[GM] Spawned robot pawn: %s (class %s)"),
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
// Commands
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

void AGM_Simulation::AddCheckpoint(FVector WorldLocation)
{
    if (CurrentState != ESimulationState::Planning)
    {
        LogScreen(TEXT("Can't add checkpoints while running."), FColor::Red);
        return;
    }

    // Snap clicks to the navmesh so the vehicle stays on-road
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        FNavLocation Proj;
        if (Nav->ProjectPointToNavigation(WorldLocation, Proj, FVector(600, 600, 800)))
            WorldLocation = Proj.Location;
    }

    CheckpointLocations.Add(WorldLocation);
    RefreshMarkers();
}

void AGM_Simulation::ClearCheckpoints()
{
    if (CurrentState != ESimulationState::Planning) return;

    CheckpointLocations.Empty();

    for (TWeakObjectPtr<AActor>& M : CheckpointMarkers)
        if (M.IsValid()) M->Destroy();
    CheckpointMarkers.Empty();
}

void AGM_Simulation::RemoveLastCheckpoint()
{
    if (CurrentState != ESimulationState::Planning) return;

    if (CheckpointLocations.Num() > 0)
        CheckpointLocations.RemoveAt(CheckpointLocations.Num() - 1);

    if (CheckpointMarkers.Num() > 0)
    {
        if (CheckpointMarkers.Last().IsValid())
            CheckpointMarkers.Last()->Destroy();
        CheckpointMarkers.RemoveAt(CheckpointMarkers.Num() - 1);
        UpdateMarkerIndices();
    }
}

// -------------------------------------------------------------
// Modes
// -------------------------------------------------------------
void AGM_Simulation::SetupPlanningMode()
{
    RobotPawn = ResolveOrSpawnRobotPawn();

    if (RobotPawn && RobotPawn->IsPatrolling())
        RobotPawn->EndMission();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
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
            FViewTargetTransitionParams P; P.BlendTime = 1.f; P.BlendFunction = VTBlend_Cubic;
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

    RefreshMarkers();

    if (MapOverviewWidget) MapOverviewWidget->SetVisibility(ESlateVisibility::Visible);
    if (RobotStatsWidget)  RobotStatsWidget->SetVisibility(ESlateVisibility::Hidden);
    if (PatrolInfoWidget)  PatrolInfoWidget->SetVisibility(ESlateVisibility::Hidden);

    // NEW: hide camera dock in planning mode (keep instance if already created)
    if (CameraDock) CameraDock->SetVisibility(ESlateVisibility::Hidden);
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

    // --- Ensure widgets exist ---
    if (!RobotStatsWidget && RobotStatsWidgetClass && PC)
    {
        RobotStatsWidget = CreateWidget<UUserWidget>(PC, RobotStatsWidgetClass);
    }
    if (!PatrolInfoWidget && PatrolInfoWidgetClass && PC)
    {
        PatrolInfoWidget = CreateWidget<UUserWidget>(PC, PatrolInfoWidgetClass);
    }
    // NEW: create camera dock if needed
    if (!CameraDock && CameraDockClass && PC)
    {
        CameraDock = CreateWidget<UUserWidget>(PC, CameraDockClass);
        if (CameraDock)
        {
            CameraDock->AddToViewport(/*Z*/ 8);
            CameraDock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            CameraDock->SetIsEnabled(true);
        }
    }
    else if (CameraDock)
    {
        if (!CameraDock->IsInViewport())
            CameraDock->AddToViewport(8);
        CameraDock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        CameraDock->SetIsEnabled(true);
    }

    // --- Add the other widgets to viewport & make them non-blocking ---
    const int32 Z_PatrolInfo = 5;
    const int32 Z_RobotStats = 6;

    if (PatrolInfoWidget)
    {
        if (PatrolInfoWidget->IsInViewport()) PatrolInfoWidget->RemoveFromParent();
        PatrolInfoWidget->AddToViewport(Z_PatrolInfo);
        PatrolInfoWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        PatrolInfoWidget->SetIsEnabled(true);
    }

    if (RobotStatsWidget)
    {
        if (RobotStatsWidget->IsInViewport()) RobotStatsWidget->RemoveFromParent();
        RobotStatsWidget->AddToViewport(Z_RobotStats);
        RobotStatsWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        RobotStatsWidget->SetIsEnabled(true);
    }

    // Feed checkpoints + ensure 3P camera is active
    RobotPawn->SetPatrolCheckpoints(CheckpointLocations);
    RobotPawn->ForceThirdPersonCamera();

    // Keep UI interactive
    PC->bAutoManageActiveCameraTarget = false;
    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    PC->bEnableMouseOverEvents = true;

    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetWidgetToFocus(nullptr);
    PC->SetInputMode(Mode);

    // Blend to the pawn’s stable view-target
    if (AActor* VT = RobotPawn->GetThirdPersonViewTarget())
    {
        FViewTargetTransitionParams P; P.BlendTime = 1.0f; P.BlendFunction = VTBlend_Cubic;
        PC->SetViewTargetWithBlend(VT, P.BlendTime, P.BlendFunction);
    }

    // Make sure keyboard binds reach the pawn while AI owns it
    RobotPawn->EnableInput(PC);

    // Start in patrol mode immediately
    RobotPawn->BeginMission();

    // Safety: re-assert camera next tick
    ReassertRobotView();
}

void AGM_Simulation::ReassertRobotView()
{
    GetWorldTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this, [this]()
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
            })
    );
}

// -------------------------------------------------------------
// Visual markers
// -------------------------------------------------------------
void AGM_Simulation::RefreshMarkers()
{
    // clear old
    for (TWeakObjectPtr<AActor>& M : CheckpointMarkers)
        if (M.IsValid()) M->Destroy();
    CheckpointMarkers.Empty();

    if (!CheckpointMarkerClass) return;

    UWorld* W = GetWorld();
    if (!W) return;

    for (int32 i = 0; i < CheckpointLocations.Num(); ++i)
    {
        const FVector L = CheckpointLocations[i];
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ACheckpointMarker* M = W->SpawnActor<ACheckpointMarker>(CheckpointMarkerClass, FTransform(L)))
        {
            M->InitMarker(i + 1);
            CheckpointMarkers.Add(M);
        }
    }
}

void AGM_Simulation::UpdateMarkerIndices()
{
    for (int32 i = 0; i < CheckpointMarkers.Num(); ++i)
        if (ACheckpointMarker* M = Cast<ACheckpointMarker>(CheckpointMarkers[i].Get()))
            M->InitMarker(i + 1);
}

void AGM_Simulation::LogScreen(const FString& Msg, FColor Col, float Time) const
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, Time, Col, Msg);
}
