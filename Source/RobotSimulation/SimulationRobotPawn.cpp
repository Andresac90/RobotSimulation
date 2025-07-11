#include "SimulationRobotPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit)
{
    // AI possession
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    PrimaryActorTick.bCanEverTick = true;

    // Engine torque setup
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

    // Gather Waypoints by tag
    if (Waypoints.Num() == 0)
    {
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("Waypoint"), Waypoints);
    }

    // Spawn HUD
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget) HUDWidget->AddToViewport();
    }

    // Bind MoveTo callback
    AICon = Cast<AAIController>(GetController());
    if (AICon && AICon->GetPathFollowingComponent())
    {
        AICon->GetPathFollowingComponent()
            ->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
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
    LastThrottleVal = Val;
    if (!bIsPatrolMode && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
    {
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetThrottleInput(Val);
    }
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    LastSteeringVal = Val;
    if (!bIsPatrolMode && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
    {
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetSteeringInput(Val);
    }
}

void ASimulationRobotPawn::HandbrakeInput(float Val)
{
    if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        MoveComp->SetHandbrakeInput(Val > KINDA_SMALL_NUMBER);
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;
    CurrentWPIndex = 0;

    if (AICon)
    {
        if (bIsPatrolMode && Waypoints.Num() > 0)
            AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
        else
            AICon->StopMovement();
    }

    UE_LOG(LogTemp, Log, TEXT("Patrol mode: %s"), bIsPatrolMode ? TEXT("ON") : TEXT("OFF"));
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID, const FPathFollowingResult& Result)
{
    if (!bIsPatrolMode || Waypoints.Num() == 0 || !AICon)
        return;

    CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
    AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
}
