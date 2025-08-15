#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "SimulationRobotPawn.generated.h"

class AWaypoint;
class AAIController;
class ACameraActor;
class UThreatComponent;

/**
 * Drivable/AI patrolling vehicle with third-person & aerial cameras,
 * waypoint patrol, UI hooks, and simple threat sensing.
 */
UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);
    virtual void Tick(float DeltaTime) override;

    // --- Controls & camera ---
    UFUNCTION(BlueprintCallable, Category = "Control") void ToggleSpeedLimit();
    UFUNCTION(BlueprintCallable, Category = "Patrol")  void TogglePatrolMode();
    UFUNCTION(BlueprintCallable, Category = "Lights")  void ToggleLights();
    UFUNCTION(BlueprintCallable, Category = "Mission") void BeginMission();
    UFUNCTION(BlueprintCallable, Category = "Mission") void EndMission();
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ChangeView();

    // Force 3P camera active on the pawn
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ForceThirdPersonCamera();

    // Stable actor to use as the PC's view target (attached to spring arm)
    UFUNCTION(BlueprintPure, Category = "Camera")
    AActor* GetThirdPersonViewTarget() const { return (AActor*)ViewTargetProxy; }

    // --- HUD data ---
    UPROPERTY(BlueprintReadOnly, Category = "Stats") float SpeedKmh = 0.f;
    UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0")) float MaxSpeedKmh = 5.f;

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void OnUpdateHUD(float InSpeedKmh, bool bSpeedLimitedStatus, bool bLightsOnStatus,
        bool bPatrolModeStatus, int32 ThreatsCount, int32 CurrentWPDisplayIndex, int32 TotalWPCount);

    // --- Planning helpers ---
    UFUNCTION(BlueprintCallable, Category = "Camera") void SetAerialView(bool bUseAerial);
    UFUNCTION(BlueprintPure, Category = "Patrol") bool IsPatrolling() const { return bIsPatrolMode; }
    UFUNCTION(BlueprintCallable, Category = "Patrol") void SetPatrolCheckpoints(const TArray<FVector>& CheckpointLocations);
    UFUNCTION(BlueprintCallable, Category = "Utility") bool ScreenToWorldLocation(FVector2D ScreenPosition, FVector& WorldLocation);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Input helpers
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);
    void LookUp(float Val);
    void Turn(float Val);
    void StartCameraRotate();
    void StopCameraRotate();

    // Patrol helpers
    void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);
    void ApplySpeedLimit();
    void AdvanceToNextPatrolTarget();
    void IssueMoveToCurrentTarget();

private:
    // --- Patrol / AI state ---
    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    bool bIsPatrolMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    int32 CurrentWPIndex = 0;

    UPROPERTY() TArray<AActor*> Waypoints;
    UPROPERTY() AAIController* AICon = nullptr;

    // --- Cameras ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* ThirdPersonCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* AerialCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    float CameraBlendTime = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Control", meta = (AllowPrivateAccess = "true"))
    float LookUpSpeed = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Control", meta = (AllowPrivateAccess = "true"))
    float TurnSpeed = 90.f;

    bool bUsingAerialView = false;
    bool bRotatingCamera = false;

    // Stable Actor used as PC’s view target (attached to SpringArm)
    UPROPERTY() ACameraActor* ViewTargetProxy = nullptr;
    void EnsureViewTargetProxy();

    // --- Lights / speed ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* Headlight;

    UPROPERTY(BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    bool bLightsOn = true;

    UPROPERTY(BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = "true"))
    bool bSpeedLimited = false;

    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 100.f;

    // Dynamic checkpoints
    TArray<FVector> PatrolCheckpoints;

    // --- Threat sensing ---
    UPROPERTY(VisibleAnywhere, Category = "Threats")
    USphereComponent* ThreatSensor;

    UPROPERTY(EditAnywhere, Category = "Threats")
    float ThreatSenseRadius = 1200.f;

    UPROPERTY(EditAnywhere, Category = "Threats")
    bool bDrawThreatBoxes = true;

    TSet<TWeakObjectPtr<AActor>> NearbyThreats;

    // NOTE: UFUNCTION params must be NAMED for UHT
    UFUNCTION()
    void OnThreatBegin(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex,
        bool                 bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnThreatEnd(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                OtherBodyIndex);

    void DrawThreatDebug();
};
