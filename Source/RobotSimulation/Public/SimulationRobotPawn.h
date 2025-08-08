#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITypes.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SpotLightComponent.h"
#include "SimulationRobotPawn.generated.h"

class AWaypoint;
class AAIController;
class ACameraActor;

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Control") void ToggleSpeedLimit();
    UFUNCTION(BlueprintCallable, Category = "Patrol")  void TogglePatrolMode();
    UFUNCTION(BlueprintCallable, Category = "Lights")  void ToggleLights();
    UFUNCTION(BlueprintCallable, Category = "Mission") void BeginMission();
    UFUNCTION(BlueprintCallable, Category = "Mission") void EndMission();
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ChangeView();

    // Used by GameMode to make sure the 3P cam is active
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ForceThirdPersonCamera();

    // ✅ Returns an actor the PlayerController should view for 3rd-person (a CameraActor attached to the spring arm)
    UFUNCTION(BlueprintCallable, Category = "Camera")
    AActor* GetThirdPersonViewTarget();

    UPROPERTY(BlueprintReadOnly, Category = "Stats") float SpeedKmh = 0.f;
    UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0")) float MaxSpeedKmh = 5.f;

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void OnUpdateHUD(float InSpeedKmh, bool bSpeedLimitedStatus, bool bLightsOnStatus,
        bool bPatrolModeStatus, int32 TreatsCount, int32 CurrentWPDisplayIndex, int32 TotalWPCount);

    // planning helpers
    UFUNCTION(BlueprintCallable, Category = "Camera") void SetAerialView(bool bUseAerial);
    UFUNCTION(BlueprintPure, Category = "Patrol") bool IsPatrolling() const { return bIsPatrolMode; }
    UFUNCTION(BlueprintCallable, Category = "Patrol") void SetPatrolCheckpoints(const TArray<FVector>& CheckpointLocations);
    UFUNCTION(BlueprintCallable, Category = "Utility") bool ScreenToWorldLocation(FVector2D ScreenPosition, FVector& WorldLocation);

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Ensure a valid camera when the pawn is used as ViewTarget (kept for completeness)
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

    // inputs & helpers
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);
    void LookUp(float Val);
    void Turn(float Val);
    void StartCameraRotate();
    void StopCameraRotate();

    void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);
    void ApplySpeedLimit();
    void AdvanceToNextPatrolTarget();

    // Builds/updates the attached CameraActor that the PC will view
    ACameraActor* EnsureFollowCameraActor();

private:
    // patrol
    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    bool bIsPatrolMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    int32 CurrentWPIndex = 0;

    UPROPERTY() TArray<AActor*> Waypoints;
    UPROPERTY() AAIController* AICon = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
    int32 TreatsDetected = 0;

    // camera
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

    // lights/speed
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* Headlight;

    UPROPERTY(BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    bool bLightsOn = true;

    UPROPERTY(BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = "true"))
    bool bSpeedLimited = false;

    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 200.f;

    // dynamic checkpoints
    TArray<FVector> PatrolCheckpoints;

    // The dedicated camera actor we attach to the spring arm and use as the PC's view target
    UPROPERTY() ACameraActor* FollowCamActor = nullptr;
};
