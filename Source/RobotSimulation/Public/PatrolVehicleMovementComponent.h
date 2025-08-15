// .h
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
    virtual void RequestPathMove(const FVector& MoveInput) override { RequestDirectMove(MoveInput, false); }
    virtual bool CanStartPathFollowing() const override { return GetOwner() != nullptr; }
    virtual void StopActiveMovement() override;

    FVector GetPathFollowingAgentLocation() const { return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector; }

    UPROPERTY(EditAnywhere, Category = "AI|Cornering") float CornerSlowStartAngleDeg = 20.f;
    UPROPERTY(EditAnywhere, Category = "AI|Cornering") float CornerSlowMaxAngleDeg = 60.f;
    UPROPERTY(EditAnywhere, Category = "AI|Cornering", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float MinThrottleWhenTurning = 0.25f;
};
