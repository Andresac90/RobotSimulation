#include "PatrolVehicleMovementComponent.h"
#include "Engine/Engine.h"

UPatrolVehicleMovementComponent::UPatrolVehicleMovementComponent()
{

}

void UPatrolVehicleMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool /*bForceMaxSpeed*/)
{
    if (MoveVelocity.IsNearlyZero())
    {
        // Stop the vehicle if no movement is requested
        SetThrottleInput(0.0f);
        SetSteeringInput(0.0f);
        return;
    }

    FVector Dir = MoveVelocity.GetSafeNormal();
    FVector ForwardVector = GetOwner()->GetActorForwardVector();
    FVector RightVector = GetOwner()->GetActorRightVector();

    // Calculate forward and right components
    float Forward = FVector::DotProduct(ForwardVector, Dir);
    float Right = FVector::DotProduct(RightVector, Dir);

    // Apply throttle (only forward, no reverse for simplicity)
    SetThrottleInput(FMath::Clamp(Forward, -1.0f, 1.0f));

    // Apply steering
    SetSteeringInput(FMath::Clamp(Right, -1.0f, 1.0f));

    // Debug logging
    UE_LOG(LogTemp, VeryVerbose, TEXT("RequestDirectMove: Forward=%f, Right=%f, MoveVel=%s"),
        Forward, Right, *MoveVelocity.ToString());
}

void UPatrolVehicleMovementComponent::RequestPathMove(const FVector& MoveInput)
{
    // For vehicles, we typically handle this the same as RequestDirectMove
    RequestDirectMove(MoveInput, false);
}

bool UPatrolVehicleMovementComponent::CanStartPathFollowing() const
{
    // Return true if the vehicle is ready to follow paths
    return GetOwner() != nullptr && IsValid(GetOwner());
}

void UPatrolVehicleMovementComponent::StopActiveMovement()
{
    // Stop all movement inputs
    SetThrottleInput(0.0f);
    SetSteeringInput(0.0f);
    SetHandbrakeInput(true);

    UE_LOG(LogTemp, Log, TEXT("StopActiveMovement called"));
}

FVector UPatrolVehicleMovementComponent::GetPathFollowingAgentLocation() const
{
    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}