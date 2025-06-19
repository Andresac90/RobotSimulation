// SimulationRobotPawn.cpp
#include "SimulationRobotPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit)
{
    PrimaryActorTick.bCanEverTick = true;
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
    if (!bPatrolMode)
    {
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetThrottleInput(Val);
    }
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    if (!bPatrolMode)
    {
        if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
            MoveComp->SetSteeringInput(Val);
    }
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bPatrolMode = !bPatrolMode;
    CurrentWPIndex = 0;
    UE_LOG(LogTemp, Log, TEXT("Patrol mode: %s"), bPatrolMode ? TEXT("ON") : TEXT("OFF"));
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bPatrolMode && Waypoints.Num())
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

    // Always throttle forward (abs), flip steering if target is behind
    float ThrottleVal = FMath::Abs(ForwardDot);
    float SteeringVal = (ForwardDot >= 0) ? RightDot : -RightDot;

    if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        MoveComp->SetThrottleInput(ThrottleVal);
        MoveComp->SetSteeringInput(SteeringVal);
    }

    if (Dist < AcceptanceRadius)
        CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
}
