#include "PatrolVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"

UPatrolVehicleMovementComponent::UPatrolVehicleMovementComponent()
{
}

void UPatrolVehicleMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool /*bForceMaxSpeed*/)
{
    if (MoveVelocity.IsNearlyZero())
    {
        SetThrottleInput(0.0f);
        SetSteeringInput(0.0f);
        return;
    }

    const FVector Dir = MoveVelocity.GetSafeNormal();
    const FVector ForwardVector = GetOwner()->GetActorForwardVector();
    const FVector RightVector = GetOwner()->GetActorRightVector();

    const float Forward = FVector::DotProduct(ForwardVector, Dir);
    const float Right = FVector::DotProduct(RightVector, Dir);

    // --- Corner-aware throttle scaling ---
    const float CosAngle = FMath::Clamp(Forward, -1.f, 1.f);
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));

    float SlowAlpha = FMath::GetRangePct(CornerSlowStartAngleDeg, CornerSlowMaxAngleDeg, AngleDeg);
    SlowAlpha = FMath::Clamp(SlowAlpha, 0.f, 1.f);

    const float ThrottleScale = FMath::Lerp(1.f, MinThrottleWhenTurning, SlowAlpha);

    SetThrottleInput(FMath::Clamp(Forward, -1.f, 1.f) * ThrottleScale);
    SetSteeringInput(FMath::Clamp(Right, -1.f, 1.f));

    // Optional: add a touch of braking if turning extremely hard (keeps it glued to path)
    const bool bVeryHardTurn = AngleDeg >= CornerSlowMaxAngleDeg * 0.9f;
    SetHandbrakeInput(bVeryHardTurn);
}

void UPatrolVehicleMovementComponent::RequestPathMove(const FVector& MoveInput)
{
    RequestDirectMove(MoveInput, false);
}

bool UPatrolVehicleMovementComponent::CanStartPathFollowing() const
{
    return GetOwner() != nullptr && IsValid(GetOwner());
}

void UPatrolVehicleMovementComponent::StopActiveMovement()
{
    SetThrottleInput(0.0f);
    SetSteeringInput(0.0f);
    SetHandbrakeInput(true);
}

FVector UPatrolVehicleMovementComponent::GetPathFollowingAgentLocation() const
{
    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}
