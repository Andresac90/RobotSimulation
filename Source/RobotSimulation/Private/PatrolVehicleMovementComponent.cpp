// .cpp
#include "PatrolVehicleMovementComponent.h"
#include "GameFramework/Actor.h"

UPatrolVehicleMovementComponent::UPatrolVehicleMovementComponent() {}

void UPatrolVehicleMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool /*bForceMaxSpeed*/)
{
    if (MoveVelocity.IsNearlyZero())
    {
        SetThrottleInput(0.f); SetSteeringInput(0.f); return;
    }

    const FVector Dir = MoveVelocity.GetSafeNormal();
    const FVector Fwd = GetOwner()->GetActorForwardVector();
    const FVector Rt = GetOwner()->GetActorRightVector();

    const float Forward = FVector::DotProduct(Fwd, Dir);
    const float Right = FVector::DotProduct(Rt, Dir);

    const float CosAngle = FMath::Clamp(Forward, -1.f, 1.f);
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));
    float SlowAlpha = FMath::Clamp(FMath::GetRangePct(CornerSlowStartAngleDeg, CornerSlowMaxAngleDeg, AngleDeg), 0.f, 1.f);
    const float ThrottleScale = FMath::Lerp(1.f, MinThrottleWhenTurning, SlowAlpha);

    SetThrottleInput(FMath::Clamp(Forward, -1.f, 1.f) * ThrottleScale);
    SetSteeringInput(FMath::Clamp(Right, -1.f, 1.f));
    SetHandbrakeInput(AngleDeg >= CornerSlowMaxAngleDeg * 0.9f);
}

void UPatrolVehicleMovementComponent::StopActiveMovement()
{
    SetThrottleInput(0.f); SetSteeringInput(0.f); SetHandbrakeInput(true);
}
