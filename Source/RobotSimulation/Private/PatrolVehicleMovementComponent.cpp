#include "PatrolVehicleMovementComponent.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"

UPatrolVehicleMovementComponent::UPatrolVehicleMovementComponent() {}

void UPatrolVehicleMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool /*bForceMaxSpeed*/)
{
    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

    if (MoveVelocity.IsNearlyZero())
    {
        SmoothedThrottle = FMath::FInterpTo(SmoothedThrottle, 0.f, Dt, ThrottleSmoothing);
        SmoothedSteer = FMath::FInterpTo(SmoothedSteer, 0.f, Dt, SteeringSmoothing);
        SetThrottleInput(SmoothedThrottle);
        SetSteeringInput(SmoothedSteer);
        SetHandbrakeInput(false);
        return;
    }

    const FVector Loc = GetOwner()->GetActorLocation();

    // Desired forward direction
    FVector DesiredDir = MoveVelocity.GetSafeNormal();

    // Soft correction toward nearest navmesh point (stay in corridor)
    if (bNavCorridorCorrection)
    {
        if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            FNavLocation Proj;
            if (Nav->ProjectPointToNavigation(Loc, Proj, FVector(NavProjectExtentXY, NavProjectExtentXY, NavProjectExtentZ)))
            {
                const FVector Corr = (Proj.Location - Loc);
                const float   Dist = Corr.Size2D();
                if (Dist > 50.f)
                {
                    const FVector CorrDir = Corr.GetSafeNormal2D();
                    const float   CorrAmt = FMath::Clamp(Dist / 500.f, 0.f, NavCorridorMaxForce);
                    DesiredDir = (DesiredDir + CorrDir * (NavCorridorStrength * CorrAmt)).GetSafeNormal();
                }
            }
        }
    }

    const FVector Fwd = GetOwner()->GetActorForwardVector();
    const FVector Rt = GetOwner()->GetActorRightVector();

    const float Forward = FVector::DotProduct(Fwd, DesiredDir);
    const float Right = FVector::DotProduct(Rt, DesiredDir);

    const float CosAngle = FMath::Clamp(Forward, -1.f, 1.f);
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));
    const float SlowAlpha = FMath::Clamp(
        FMath::GetRangePct(CornerSlowStartAngleDeg, CornerSlowMaxAngleDeg, AngleDeg),
        0.f, 1.f);

    const float ThrottleScale = FMath::Lerp(1.f, MinThrottleWhenTurning, SlowAlpha);
    const float CurrSpeedKmh = GetForwardSpeed() * 0.036f;
    const float TargetSpeedKmh = FMath::Lerp(MaxSpeedKmh, CornerMaxSpeedKmh, SlowAlpha);
    bool bTooFast = CurrSpeedKmh > TargetSpeedKmh + 0.5f;

    // throttle command (no instant braking: let it coast)
    float DesiredThrottle = FMath::Clamp(Forward, -1.f, 1.f) * ThrottleScale;
    if (bTooFast) DesiredThrottle = FMath::Min(DesiredThrottle, 0.f);

    // smooth steering + throttle
    const float DesiredSteer = FMath::Clamp(Right, -1.f, 1.f);
    SmoothedSteer = FMath::FInterpTo(SmoothedSteer, DesiredSteer, Dt, SteeringSmoothing);
    SmoothedThrottle = FMath::FInterpTo(SmoothedThrottle, DesiredThrottle, Dt, ThrottleSmoothing);

    SetSteeringInput(SmoothedSteer);
    SetThrottleInput(SmoothedThrottle);

    // Handbrake only at VERY sharp angles as a last resort
    const bool bVerySharp = AngleDeg >= CornerSlowMaxAngleDeg * 0.95f;
    SetHandbrakeInput(bVerySharp && bTooFast);
}

void UPatrolVehicleMovementComponent::StopActiveMovement()
{
    SmoothedThrottle = 0.f;
    SmoothedSteer = 0.f;
    SetThrottleInput(0.f);
    SetSteeringInput(0.f);
    SetHandbrakeInput(true);
}
