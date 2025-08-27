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
class UThreatBoxesWidget;

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);
    virtual void Tick(float DeltaTime) override;

    // Controls & camera
    UFUNCTION(BlueprintCallable, Category = "Control") void ToggleSpeedLimit();
    UFUNCTION(BlueprintCallable, Category = "Patrol")  void TogglePatrolMode();
    UFUNCTION(BlueprintCallable, Category = "Lights")  void ToggleLights();
    UFUNCTION(BlueprintCallable, Category = "Mission") void BeginMission();
    UFUNCTION(BlueprintCallable, Category = "Mission") void EndMission();
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ChangeView();            // 3P <-> Aerial
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ToggleVehicleCamera();   // 3P <-> Interior
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ForceThirdPersonCamera();

    UFUNCTION(BlueprintPure, Category = "Camera")
    AActor* GetThirdPersonViewTarget() const { return (AActor*)ViewTargetProxy; }

    // HUD
    UPROPERTY(BlueprintReadOnly, Category = "Stats") float SpeedKmh = 0.f;
    UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0")) float MaxSpeedKmh = 5.f;

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void OnUpdateHUD(float InSpeedKmh, bool bSpeedLimitedStatus, bool bLightsOnStatus,
        bool bPatrolModeStatus, int32 ThreatsCount, int32 CurrentWPDisplayIndex, int32 TotalWPCount);

    UFUNCTION(BlueprintPure, Category = "Threats")
    int32 GetThreatCount() const { return NearbyThreats.Num(); }

    // Planning helpers
    UFUNCTION(BlueprintCallable, Category = "Camera")  void SetAerialView(bool bUseAerial);
    UFUNCTION(BlueprintPure, Category = "Patrol")    bool IsPatrolling() const { return bIsPatrolMode; }
    UFUNCTION(BlueprintCallable, Category = "Patrol")  void SetPatrolCheckpoints(const TArray<FVector>& CheckpointLocations);
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

    void BuildAndFollowCenterPathTo(const FVector& GoalLocation);
    void FollowNextSubPoint();
    void IssueMoveToCurrentTarget();

private:
    // ---------- UI / Input mode ----------
    void ApplyAlwaysInteractiveInput(APlayerController* PC); // mouse always visible + click events
    void InstallAuxInput(APlayerController* PC);             // keep key binds alive while AI possesses
    void RemoveAuxInput(APlayerController* PC);
    void BindCommonInputs(class UInputComponent* IC);

    // Try to recover components if a BP child renamed/removed them
    void ResolveCriticalComponents();

    // ---------- Cameras ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArm = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* ThirdPersonCamera = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* AerialCamera = nullptr;

    // Interior camera (yaw only, via Turn axis)
    UPROPERTY(VisibleAnywhere, Category = "Camera|Interior")
    USceneComponent* InteriorPivot = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Camera|Interior")
    UCameraComponent* InteriorCamera = nullptr;

    UPROPERTY(EditAnywhere, Category = "Camera")
    float CameraBlendTime = 1.f;

    // 3P camera distance & smoothing
    UPROPERTY(EditAnywhere, Category = "Camera") float ThirdPersonArmLength = 1200.f;
    UPROPERTY(EditAnywhere, Category = "Camera") bool  bEnableCamLag = true;
    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0")) float CamLagSpeed = 10.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Control") float LookUpSpeed = 45.f;
    UPROPERTY(EditAnywhere, Category = "Camera|Control") float TurnSpeed = 90.f;
    UPROPERTY(EditAnywhere, Category = "Camera|Interior") float InteriorTurnSpeed = 120.f;

    bool bUsingAerialView = false;
    bool bUsingInteriorView = false;
    bool bRotatingCamera = false;

    UPROPERTY() ACameraActor* ViewTargetProxy = nullptr;
    void EnsureViewTargetProxy();

    // ---------- Patrol / AI ----------
    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    bool bIsPatrolMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    int32 CurrentWPIndex = 0;

    UPROPERTY() TArray<AActor*> Waypoints;
    UPROPERTY() AAIController* AICon = nullptr;

    UPROPERTY(EditAnywhere, Category = "Patrol") float AcceptanceRadius = 100.f;

    // Path smoothing (round corners so we don’t hug the inside)
    UPROPERTY(EditAnywhere, Category = "Patrol|Path")
    float CornerRadius = 400.f;

    UPROPERTY(EditAnywhere, Category = "Patrol|Path")
    float MinCornerAngleDeg = 10.f;

    // Dynamic checkpoints
    TArray<FVector> PatrolCheckpoints;

    // ---------- Lights / speed ----------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* HeadlightLeft = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* HeadlightRight = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    bool bLightsOn = true;

    UPROPERTY(BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = "true"))
    bool bSpeedLimited = false;

    // ---------- Threat sensing ----------
    UPROPERTY(VisibleAnywhere, Category = "Threats") USphereComponent* ThreatSensor = nullptr;
    UPROPERTY(EditAnywhere, Category = "Threats")  float ThreatSenseRadius = 1200.f;
    UPROPERTY(EditAnywhere, Category = "Threats")  bool  bDrawThreatBoxes = true;

    UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<UThreatBoxesWidget> ThreatOverlayWidgetClass;
    UPROPERTY() UThreatBoxesWidget* ThreatOverlayWidget = nullptr;

    TSet<TWeakObjectPtr<AActor>> NearbyThreats;

    UFUNCTION()
    void OnThreatBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnThreatEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void DrawThreatDebug();
    void UpdateThreatOverlay();

    // Center-path follow (smoothed points)
    TArray<FVector> ActivePathPoints;
    int32 ActivePathIndex = INDEX_NONE;
    bool  bFollowingSubPath = false;

    // helpers
    void SmoothCorners(const TArray<FVector>& InPoints, TArray<FVector>& OutPoints) const;

    // Aux input to keep keybinds when AI possesses
    UPROPERTY() UInputComponent* AuxInput = nullptr;
};
