#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SimulationRobotPawn.generated.h"

class AWaypoint;
class ACameraActor;
class UUserWidget;
class UThreatBoxesWidget;
class UThreatComponent;

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);
    virtual void Tick(float DeltaTime) override;

    // ---------- UI-callable controls ----------
    UFUNCTION(BlueprintCallable, Category = "Control") void ToggleSpeedLimit();
    UFUNCTION(BlueprintCallable, Category = "Patrol")  void TogglePatrolMode();
    UFUNCTION(BlueprintCallable, Category = "Lights")  void ToggleLights();
    UFUNCTION(BlueprintCallable, Category = "Mission") void BeginMission();
    UFUNCTION(BlueprintCallable, Category = "Mission") void EndMission();
    UFUNCTION(BlueprintCallable, Category = "Mission") void EndSimulation();
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ChangeView();
    UFUNCTION(BlueprintCallable, Category = "Camera")  void ForceThirdPersonCamera();

    // ---------- Helpers ----------
    UFUNCTION(BlueprintCallable, Category = "Camera")  void SetAerialView(bool bUseAerial);
    UFUNCTION(BlueprintPure, Category = "Patrol")    bool IsPatrolling() const { return bIsPatrolMode; }
    UFUNCTION(BlueprintCallable, Category = "Patrol")  void SetPatrolCheckpoints(const TArray<FVector>& CheckpointLocations);
    UFUNCTION(BlueprintCallable, Category = "Utility") bool ScreenToWorldLocation(FVector2D ScreenPosition, FVector& WorldLocation);

    // -------- Camera feeds for UI --------
    UFUNCTION(BlueprintPure, Category = "Camera|Feeds") UTextureRenderTarget2D* GetCam360RT() const { return Cam360RT; }
    UFUNCTION(BlueprintPure, Category = "Camera|Feeds") UTextureRenderTarget2D* GetFrontRT() const { return FrontRT; }
    UFUNCTION(BlueprintPure, Category = "Camera|Feeds") UTextureRenderTarget2D* GetRearRT()  const { return RearRT; }
    UFUNCTION(BlueprintPure, Category = "Camera") AActor* GetThirdPersonViewTarget() const { return (AActor*)ViewTargetProxy; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Input helpers
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);
    void LookUp(float Val);
    void Turn(float Val);
    void StartCameraRotate();
    void StopCameraRotate();

private:
    // ---------- UI / Input mode ----------
    void ApplyAlwaysInteractiveInput(APlayerController* PC);
    void InstallAuxInput(APlayerController* PC);
    void RemoveAuxInput(APlayerController* PC);
    void BindCommonInputs(class UInputComponent* IC);
    void ResolveCriticalComponents();

    // ---------- Cameras ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArm = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* ThirdPersonCamera = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* AerialCamera = nullptr;

    // Feeds for UI
    UPROPERTY(VisibleAnywhere, Category = "Camera|Feeds") USceneComponent* InteriorPivot = nullptr;
    UPROPERTY(VisibleAnywhere, Category = "Camera|Feeds") USceneCaptureComponent2D* Cam360Capture = nullptr;
    UPROPERTY(BlueprintReadOnly, Category = "Camera|Feeds", meta = (AllowPrivateAccess = "true")) UTextureRenderTarget2D* Cam360RT = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Camera|Feeds") USceneCaptureComponent2D* RearCapture = nullptr;
    UPROPERTY(BlueprintReadOnly, Category = "Camera|Feeds", meta = (AllowPrivateAccess = "true")) UTextureRenderTarget2D* RearRT = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Camera|Feeds") USceneCaptureComponent2D* FrontCapture = nullptr;
    UPROPERTY(BlueprintReadOnly, Category = "Camera|Feeds", meta = (AllowPrivateAccess = "true")) UTextureRenderTarget2D* FrontRT = nullptr;

    // 3P camera tuning
    UPROPERTY(EditAnywhere, Category = "Camera") float ThirdPersonArmLength = 1200.f;
    UPROPERTY(EditAnywhere, Category = "Camera") bool  bEnableCamLag = true;
    UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0")) float CamLagSpeed = 10.f;
    UPROPERTY(EditAnywhere, Category = "Camera") float CameraBlendTime = 1.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Control") float LookUpSpeed = 45.f;
    UPROPERTY(EditAnywhere, Category = "Camera|Control") float TurnSpeed = 90.f;

    bool bUsingAerialView = false;
    bool bRotatingCamera = false;

    UPROPERTY() ACameraActor* ViewTargetProxy = nullptr;
    void EnsureViewTargetProxy();

    // 360 auto-pan
    UPROPERTY(EditAnywhere, Category = "Camera|Feeds") float Cam360YawAmplitude = 75.f; // +/- deg
    UPROPERTY(EditAnywhere, Category = "Camera|Feeds") float Cam360YawSpeed = 20.f;     // deg/sec
    float Cam360Time = 0.f;

public:
    // ---------- HUD-facing stats ----------
    UPROPERTY(BlueprintReadOnly, Category = "Stats") float SpeedKmh = 0.f;
    UPROPERTY(EditAnywhere, Category = "Control", meta = (ClampMin = "0.0")) float MaxSpeedKmh = 22.f;

    // Threat counters
    UFUNCTION(BlueprintPure, Category = "Threats") int32 GetThreatCount() const;                 // cumulative (never decreases)
    UFUNCTION(BlueprintPure, Category = "Threats") int32 GetActiveThreatsInRange() const { return NearbyThreats.Num(); }
    UFUNCTION(BlueprintPure, Category = "Threats") int32 GetUniqueThreatsSeen()   const { return AllThreatsEverSeen.Num(); }
    UFUNCTION(BlueprintCallable, Category = "Threats") void ResetThreatCounters();

private:
    // ---------- Patrol / Path ----------
    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true")) bool  bIsPatrolMode = false;
    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true")) int32 CurrentWPIndex = 0; // current goal index (0 first)

    UPROPERTY() TArray<AActor*> Waypoints;
    UPROPERTY(EditAnywhere, Category = "Patrol") float GoalAcceptanceRadius = 150.f;

    UPROPERTY(EditAnywhere, Category = "Patrol|Path") float CorridorStep = 120.f;
    UPROPERTY(EditAnywhere, Category = "Patrol|Path") float MinPointSpacing = 60.f;
    UPROPERTY(EditAnywhere, Category = "Patrol|Path") float SmoothKernel = 0.25f;

    TArray<FVector> ActivePathPoints;
    TArray<float>   ActivePathCumLen;
    float           ActivePathTotalLen = 0.f;
    int32           CachedClosestSeg = 0;

    // Pure Pursuit / vehicle geometry
    UPROPERTY(EditAnywhere, Category = "AI|Control") float WheelbaseCM = 260.f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float MaxSteerAngleDeg = 50.f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float LookaheadMinCM = 300.f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float LookaheadGainPerKmh = 25.f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float LookaheadMaxCM = 1600.f;

    UPROPERTY(EditAnywhere, Category = "AI|Control") float LateralAccelMax = 3.5f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float CoastBandKmh = 2.0f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float BrakeBandKmh = 6.0f;
    UPROPERTY(EditAnywhere, Category = "AI|Response") float SteeringSmoothing = 6.f;
    UPROPERTY(EditAnywhere, Category = "AI|Response") float ThrottleSmoothing = 3.8f;
    UPROPERTY(EditAnywhere, Category = "AI|Response") float BrakeSmoothing = 4.8f;
    float SmoothedSteer = 0.f, SmoothedThrottle = 0.f, SmoothedBrake = 0.f;
    float FilteredDesiredSpeedKmh = 0.f;

    // Checkpoint visuals
    UPROPERTY(EditAnywhere, Category = "Patrol|Visual") TSubclassOf<AActor> CheckpointMarkerClass;
    UPROPERTY(EditAnywhere, Category = "Patrol|Visual") FName               CheckpointMarkerTag;
    void SetCheckpointMeshesHidden(bool bHide);

    // ---------- Lights ----------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* HeadlightLeft = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* HeadlightRight = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* TailLightLeft = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* TailLightRight = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true")) bool bHeadlightsOn = false;

    void SetHeadlightsOn(bool bOn);
    void ApplyHeadlightVisibility();
    void UpdateBrakeLightState(bool bBraking);
    bool bBrakingNow = false;
    bool bHandbrakeActiveManual = false;

    // ---------- Threat / UI ----------
    UPROPERTY(VisibleAnywhere, Category = "Threats") USphereComponent* ThreatSensor = nullptr;
    UPROPERTY(EditAnywhere, Category = "Threats", meta = (ClampMin = "50.0")) float ThreatSenseRadius = 1200.f;

    // Separate controls for debug vs UI overlay
    /** Show threat overlay widget in all builds (including Shipping) */
    UPROPERTY(EditAnywhere, Category = "Threats", meta = (DisplayName = "Show Threat Overlay"))
    bool bShowThreatOverlay = true;

    /** Show debug boxes in Development/Debug builds only (ignored in Shipping) */
    UPROPERTY(EditAnywhere, Category = "Threats", meta = (DisplayName = "Debug: Draw Threat Boxes"))
    bool bDrawThreatDebugBoxes = true;

    // Filtering controls
    /** If true (default), only actors with UThreatComponent are considered threats. */
    UPROPERTY(EditAnywhere, Category = "Threats")  bool  bRequireThreatComponent = true;

    /** Optional: if set (e.g. "Threat"), actor must contain this tag to be considered a threat. */
    UPROPERTY(EditAnywhere, Category = "Threats")  FName ThreatRequiredActorTag;

    UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<UUserWidget>        HUDWidgetClass;
    UPROPERTY() UUserWidget* HUDWidget = nullptr;

    UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<UThreatBoxesWidget> ThreatOverlayWidgetClass;
    UPROPERTY() UThreatBoxesWidget* ThreatOverlayWidget = nullptr;

    // In-range set
    TSet<TWeakObjectPtr<AActor>> NearbyThreats;

    // Cumulative counters/sets
    UPROPERTY(BlueprintReadOnly, Category = "Threats", meta = (AllowPrivateAccess = "true"))
    int32 TotalThreatDetections = 0;                    // increments on each qualified begin overlap
    TSet<TWeakObjectPtr<AActor>> AllThreatsEverSeen;    // unique actors ever detected

    // Overlap handlers
    UFUNCTION() void OnThreatBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    UFUNCTION() void OnThreatEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // Filter helper
    bool QualifiesAsThreat(AActor* OtherActor) const;

    void DrawThreatDebug();
    void UpdateThreatOverlay();

    // ---- Path helpers ----
    bool  BuildPathTo(const FVector& Goal);
    void  SmoothPathInPlace(TArray<FVector>& Pts, float Alpha);
    void  RecomputeCumulativeLength();
    bool  FindClosestOnPath2D(const FVector& Pos, int32& OutSeg, float& OutSegT, FVector& OutPoint) const;
    FVector SamplePathAtS(float S, int32& IO_Seg) const;
    void   DriveAlongPath(float Dt);
    bool   ProjectToNav(const FVector& In, FVector& Out) const;

    // ---------- Corridor helpers ----------
    void    MeasureCorridor(const FVector& Base, const FVector& Right2D, float MaxHalfWidth, float Step, float& OutLeft, float& OutRight) const;
    FVector CenterPointInCorridor(const FVector& Base, const FVector& Fwd2D, float MaxHalfWidth, float Step) const;

    // Aux input (axes while "AI" possesses)
    UPROPERTY() UInputComponent* AuxInput = nullptr;

    // --- Stuck handling ---
    UPROPERTY(EditAnywhere, Category = "AI|Stuck") float StuckSpeedKmh = 0.8f;
    UPROPERTY(EditAnywhere, Category = "AI|Stuck") float StuckReplanTimeSec = 2.0f;
    float StuckAccum = 0.f;

    // --- Hard clamp to navmesh ---
    UPROPERTY(EditAnywhere, Category = "AI|Nav") bool  bHardClampToNavmesh = true;
    UPROPERTY(EditAnywhere, Category = "AI|Nav", meta = (ClampMin = "1.0")) float MaxOffNavDistance = 30.f;
    UPROPERTY(EditAnywhere, Category = "AI|Nav") float StrictClampDistance = 120.f;
    UPROPERTY(EditAnywhere, Category = "AI|Path") float CorridorProbeHalfWidth = 500.f;
    UPROPERTY(EditAnywhere, Category = "AI|Path") float CorridorProbeStep = 25.f;
    UPROPERTY(EditAnywhere, Category = "AI|Path") float CenteringAlpha = 0.8f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float SteerTightThreshold = 0.75f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float LookaheadTightScale = 0.55f;
    UPROPERTY(EditAnywhere, Category = "AI|Control") float MinCurveSpeedKmh = 4.f;

    // Store checkpoints
    UPROPERTY() TArray<FVector> PatrolCheckpoints;

    // Speed limit
    UPROPERTY(BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = "true")) bool bSpeedLimited = false;

    // ---- Private helpers ----
    bool HasAnyCheckpoints() const;
    void OrientFrontToward(const FVector& Target);
    void OrientFrontTowardNextCheckpoint();
};