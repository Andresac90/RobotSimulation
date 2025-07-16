#include "SimulationRobotPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit
        .SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(
            AWheeledVehiclePawn::VehicleMovementComponentName))
{
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ARobotAIController::StaticClass();

    PrimaryActorTick.bCanEverTick = true;

    // Configure engine/drive curves
    if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        M->Mass = 600.f;

        M->EngineSetup.TorqueCurve.EditorCurveData.Reset();
        M->EngineSetup.TorqueCurve.EditorCurveData.AddKey(0.f, 260.f);
        M->EngineSetup.TorqueCurve.EditorCurveData.AddKey(3000.f, 260.f);
        M->EngineSetup.MaxRPM = 3000.f;
        M->EngineSetup.EngineIdleRPM = 0.f;
        M->EngineSetup.EngineBrakeEffect = 0.2f;
        M->EngineSetup.MaxTorque = 100000.f;

        M->SteeringSetup.SteeringCurve.EditorCurveData.Reset();
        M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(0.f, 1.f);
        M->SteeringSetup.SteeringCurve.EditorCurveData.AddKey(1000.f, 0.1f);

        M->SetUseAutomaticGears(true);
        M->TransmissionSetup.ForwardGearRatios.SetNum(1);
        M->TransmissionSetup.ForwardGearRatios[0] = 10.f;
        M->TransmissionSetup.bUseAutomaticGears = true;
        M->TransmissionSetup.GearChangeTime = 0.15f;
    }
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    // 1) Gather all waypoints in the level
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Found);
    Waypoints.Empty();
    for (auto* A : Found)
        if (auto* WP = Cast<AWaypoint>(A))
            Waypoints.Add(WP);

    // 2) Sort them by PatrolOrder
    Waypoints.Sort([](const AWaypoint& A, const AWaypoint& B) {
        return A.PatrolOrder < B.PatrolOrder;
        });

    // 3) Create HUD if set
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget)
            HUDWidget->AddToViewport();
    }

    // 4) Apply the speed limit default
    ApplySpeedLimit();
}

void ASimulationRobotPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (auto* AI = Cast<AAIController>(NewController))
    {
        AICon = AI;
        UE_LOG(LogTemp, Log, TEXT("Possessed by AIController %s"), *AICon->GetName());

        // Bind our completion callback
        if (auto* PF = AICon->GetPathFollowingComponent())
        {
            PF->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
        }

        // 5) Kick off patrol
        if (Waypoints.Num() > 0)
        {
            bIsPatrolMode = true;
            CurrentWPIndex = 0;
            AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
        }
    }
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* P)
{
    Super::SetupPlayerInputComponent(P);

    P->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    P->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    P->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);

    P->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
    P->BindAction("ToggleSpeed", IE_Pressed, this, &ASimulationRobotPawn::ToggleSpeedLimit);
}

void ASimulationRobotPawn::ThrottleInput(float Val)
{
    LastThrottleVal = Val;
    if (!bIsPatrolMode)
        if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            M->SetThrottleInput(Val);
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    LastSteeringVal = Val;
    if (!bIsPatrolMode)
        if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            M->SetSteeringInput(Val);
}

void ASimulationRobotPawn::HandbrakeInput(float Val)
{
    if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        M->SetHandbrakeInput(Val > KINDA_SMALL_NUMBER);
}

void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    ApplySpeedLimit();
}

void ASimulationRobotPawn::ApplySpeedLimit()
{
    if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        M->EngineSetup.MaxRPM = bSpeedLimited ? 500.f : 3000.f;
        M->EngineSetup.TorqueCurve.EditorCurveData.Reset();
        M->EngineSetup.TorqueCurve.EditorCurveData.AddKey(0.f, bSpeedLimited ? 150.f : 260.f);
        M->EngineSetup.TorqueCurve.EditorCurveData.AddKey(
            bSpeedLimited ? 500.f : 3000.f,
            bSpeedLimited ? 150.f : 260.f
        );

        UE_LOG(LogTemp, Log,
            TEXT("Speed limit is now: %s"),
            bSpeedLimited ? TEXT("ON") : TEXT("OFF")
        );
    }
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;
    CurrentWPIndex = 0;

    if (!AICon)
    {
        UE_LOG(LogTemp, Error, TEXT("TogglePatrolMode: AICon is null"));
        return;
    }

    if (bIsPatrolMode && Waypoints.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Patrol mode enabled, going to WP[0]"));
        AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Patrol mode disabled"));
        AICon->StopMovement();
    }
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID /*RequestID*/, const FPathFollowingResult& Result)
{
    if (!Result.IsSuccess() || !bIsPatrolMode || Waypoints.Num() == 0 || !AICon)
        return;

    CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
    UE_LOG(LogTemp, Log, TEXT("Arrived; moving to WP index %d"), CurrentWPIndex);
    AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
}
