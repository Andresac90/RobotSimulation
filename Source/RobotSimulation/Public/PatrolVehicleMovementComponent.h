#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "PatrolVehicleMovementComponent.generated.h"

UCLASS()
class ROBOTSIMULATION_API UPatrolVehicleMovementComponent
    : public UChaosWheeledVehicleMovementComponent
    , public IPathFollowingAgentInterface
{
    GENERATED_BODY()

public:
    UPatrolVehicleMovementComponent();

    // IPathFollowingAgentInterface
    virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
    virtual void RequestPathMove(const FVector& MoveInput) override;
    virtual bool CanStartPathFollowing() const override;
    virtual void StopActiveMovement() override;

    FVector GetPathFollowingAgentLocation() const;

    // ↓ Helps keep the car on the lane by slowing for sharp turns
    UPROPERTY(EditAnywhere, Category = "AI|Cornering")
    float CornerSlowStartAngleDeg = 20.f;      // start easing throttle after this angle
    UPROPERTY(EditAnywhere, Category = "AI|Cornering")
    float CornerSlowMaxAngleDeg = 60.f;      // at/over this angle throttle is at MinThrottleWhenTurning
    UPROPERTY(EditAnywhere, Category = "AI|Cornering", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float MinThrottleWhenTurning = 0.25f;     // never go below this while turning (prevents stalling)
};
