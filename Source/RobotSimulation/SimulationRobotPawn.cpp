#include "SimulationRobotPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit)
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    // Torque curve so it actually moves
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

    // Auto-find waypoints by tag “Waypoint”
    if (Waypoints.Num() == 0)
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("Waypoint"), Found);
        Waypoints = Found;
    }

    // Spawn the HUD widget
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget)
            HUDWidget->AddToViewport();
    }
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    PlayerInputComponent->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    PlayerInputComponent->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
}

void ASimulationRobotPawn::ThrottleInput(float Val)
{
    UE_LOG(LogTemp, Log, TEXT("ThrottleInput called: %f"), Val);

    LastThrottleVal = Val;
    if (!bPatrolMode && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
    {
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetThrottleInput(Val);
    }
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    LastSteeringVal = Val;
    if (!bPatrolMode && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
    {
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetSteeringInput(Val);
    }
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bPatrolMode = !bPatrolMode;
    bIsPatrolMode = bPatrolMode;    // expose to UI
    CurrentWPIndex = 0;

    UE_LOG(LogTemp, Log, TEXT("Patrol mode: %s"), bPatrolMode ? TEXT("ON") : TEXT("OFF"));
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bPatrolMode && Waypoints.Num() > 0)
        PatrolTick(DeltaTime);
}

void ASimulationRobotPawn::PatrolTick(float DeltaTime)
{
    AActor* WP = Waypoints[CurrentWPIndex];
    if (!WP) return;

    FVector ToWP = WP->GetActorLocation() - GetActorLocation();
    float   Dist = ToWP.Size();
    FVector Dir = ToWP.GetSafeNormal();

    float ForwardDot = FVector::DotProduct(GetActorForwardVector(), Dir);
    float RightDot = FVector::DotProduct(GetActorRightVector(), Dir);

    float ThrottleVal = FMath::Abs(ForwardDot);
    float SteeringVal = (ForwardDot >= 0.f) ? RightDot : -RightDot;

    LastThrottleVal = ThrottleVal;
    LastSteeringVal = SteeringVal;

    if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        MoveComp->SetThrottleInput(ThrottleVal);
        MoveComp->SetSteeringInput(SteeringVal);
    }

    if (Dist < AcceptanceRadius)
        CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
}
