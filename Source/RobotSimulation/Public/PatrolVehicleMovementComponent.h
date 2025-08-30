#pragma once
#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "PatrolVehicleMovementComponent.generated.h"

// Kept for compatibility; the pawn drives itself (Pure Pursuit + corridor centering)
UCLASS()
class ROBOTSIMULATION_API UPatrolVehicleMovementComponent
    : public UChaosWheeledVehicleMovementComponent
    , public IPathFollowingAgentInterface
{
    GENERATED_BODY()
public:
    UPatrolVehicleMovementComponent();

    // IPathFollowingAgentInterface no-ops
    virtual void RequestDirectMove(const FVector& /*MoveVelocity*/, bool /*bForceMaxSpeed*/) override {}
    virtual void RequestPathMove(const FVector& MoveInput) override { RequestDirectMove(MoveInput, false); }
    virtual bool CanStartPathFollowing() const override { return GetOwner() != nullptr; }
    virtual void StopActiveMovement() override
    {
        SetThrottleInput(0.f);
        SetSteeringInput(0.f);
        SetBrakeInput(0.f);
        SetHandbrakeInput(true);
    }
};
