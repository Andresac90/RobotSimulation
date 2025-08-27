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

    // Cornering parameters
    UPROPERTY(EditAnywhere, Category = "AI|Cornering") float CornerSlowStartAngleDeg = 10.f;
    UPROPERTY(EditAnywhere, Category = "AI|Cornering") float CornerSlowMaxAngleDeg = 55.f;
    UPROPERTY(EditAnywhere, Category = "AI|Cornering", meta = (ClampMin = "0.05", ClampMax = "0.9"))
    float MinThrottleWhenTurning = 0.15f;

    // Speed limits
    UPROPERTY(EditAnywhere, Category = "AI|Speed") float MaxSpeedKmh = 20.f;
    UPROPERTY(EditAnywhere, Category = "AI|Speed") float CornerMaxSpeedKmh = 7.f;

    // Responsiveness
    UPROPERTY(EditAnywhere, Category = "AI|Response") float SteeringSmoothing = 8.f;
    UPROPERTY(EditAnywhere, Category = "AI|Response") float ThrottleSmoothing = 5.f;

    // Navmesh corridor correction (nudges back toward nav mesh)
    UPROPERTY(EditAnywhere, Category = "AI|Nav") bool  bNavCorridorCorrection = true;
    UPROPERTY(EditAnywhere, Category = "AI|Nav") float NavCorridorStrength = 0.6f;  // 0..2
    UPROPERTY(EditAnywhere, Category = "AI|Nav") float NavCorridorMaxForce = 0.75f; // cap correction
    UPROPERTY(EditAnywhere, Category = "AI|Nav") float NavProjectExtentXY = 250.f;
    UPROPERTY(EditAnywhere, Category = "AI|Nav") float NavProjectExtentZ = 500.f;

private:
    float SmoothedSteer = 0.f;
    float SmoothedThrottle = 0.f;
};
