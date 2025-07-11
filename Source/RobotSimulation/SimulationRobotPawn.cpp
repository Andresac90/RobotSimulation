#include "SimulationRobotPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit)
{
    // Let AIController possess this pawn at spawn
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    PrimaryActorTick.bCanEverTick = true;

    // Give the engine a real torque curve
    if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        MoveComp->EngineSetup.TorqueCurve.EditorCurveData.Reset();
        MoveComp->EngineSetup.TorqueCurve.EditorCurveData.AddKey(0.f, 500.f);
        MoveComp->EngineSetup.TorqueCurve.EditorCurveData.AddKey(3000.f, 300.f);
        MoveComp->EngineSetup.MaxRPM = 6000.f;
    }
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    // 1) Find every AWaypoint in the level
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Found);
        for (AActor* A : Found)
            if (auto* WP = Cast<AWaypoint>(A))
                Waypoints.Add(WP);
    }

    // 2) Sort them by their PatrolOrder property
    Waypoints.Sort([](const AWaypoint& A, const AWaypoint& B) {
        return A.PatrolOrder < B.PatrolOrder;
        });

    // 3) Spawn the HUD
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget)
            HUDWidget->AddToViewport();
    }

    // 4) Cache AIController & bind our completion callback
    AICon = Cast<AAIController>(GetController());
    if (AICon && AICon->GetPathFollowingComponent())
    {
        AICon->GetPathFollowingComponent()
            ->OnRequestFinished
            .AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
    }

    // 5) Kick off the patrol immediately (optional)
    if (AICon && Waypoints.Num() > 0)
    {
        bIsPatrolMode = true;
        CurrentWPIndex = 0;
        AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
    }
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    PlayerInputComponent->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    PlayerInputComponent->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);
    PlayerInputComponent->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
}

void ASimulationRobotPawn::ThrottleInput(float Val)
{
    //UI value
    LastThrottleVal = Val;

    if (!bIsPatrolMode && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetThrottleInput(Val);
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    //UI value
    LastSteeringVal = Val;

    if (!bIsPatrolMode && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetSteeringInput(Val);
}

void ASimulationRobotPawn::HandbrakeInput(float Val)
{
    if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        MoveComp->SetHandbrakeInput(Val > KINDA_SMALL_NUMBER);
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;
    if (!AICon) return;

    if (bIsPatrolMode && Waypoints.Num() > 0)
    {
        CurrentWPIndex = 0;
        AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
    }
    else
    {
        AICon->StopMovement();
    }
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID, const FPathFollowingResult& Result)
{
    if (!bIsPatrolMode || Waypoints.Num() == 0 || !AICon)
        return;

    // Advance & wrap
    CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
    AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
}
