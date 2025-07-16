#pragma once

#include "CoreMinimal.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "PatrolVehicleMovementComponent.generated.h"

/**
 * Custom movement component that allows AI navigation control for vehicles.
 */
UCLASS()
class ROBOTSIMULATION_API UPatrolVehicleMovementComponent
    : public UChaosWheeledVehicleMovementComponent
    , public IPathFollowingAgentInterface
{
    GENERATED_BODY()

public:
    UPatrolVehicleMovementComponent();

    // IPathFollowingAgentInterface implementation
    /** Called by PathFollowingComponent to push movement velocity */
    virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;

    /** Called by PathFollowingComponent for path-based movement */
    virtual void RequestPathMove(const FVector& MoveInput) override;

    /** Returns true if the agent can start path following */
    virtual bool CanStartPathFollowing() const override;

    /** Called when movement should be stopped */
    virtual void StopActiveMovement() override;

    /** Returns the location used by AI pathfinding */
    FVector GetPathFollowingAgentLocation() const;
};