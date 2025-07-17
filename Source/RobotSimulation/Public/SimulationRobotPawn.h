// SimulationRobotPawn.h
#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SpotLightComponent.h"
#include "Blueprint/UserWidget.h"
#include "SimulationRobotPawn.generated.h"

// Forward declarations
class AWaypoint;
class AAIController;

UCLASS()
class ROBOTSIMULATION_API ASimulationRobotPawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ASimulationRobotPawn(const FObjectInitializer& ObjInit);

    virtual void Tick(float DeltaTime) override;

    /** Toggle engine speed cap */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void ToggleSpeedLimit();

    /** Toggle AI/manual patrol */
    UFUNCTION(BlueprintCallable, Category = "Patrol")
    void TogglePatrolMode();

    /** Toggle headlights on/off */
    UFUNCTION(BlueprintCallable, Category = "Lights")
    void ToggleLights();

    /** Begin mission (starts patrol) */
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void BeginMission();

    /** End mission (stops patrol) */
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void EndMission();

    /**
     * Push fresh HUD values each frame
     * (renamed parameters to avoid hiding UHT‐generated locals)
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void OnUpdateHUD(
        float SpeedKmh,
        bool  bSpeedLimitedStatus,
        bool  bLightsOnStatus,
        bool  bPatrolModeStatus,
        int32 TreatsCount,
        int32 CurrentWPDisplayIndex,
        int32 TotalWPCount
    );

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Driving handlers
    void ThrottleInput(float Val);
    void SteeringInput(float Val);
    void HandbrakeInput(float Val);

    // Look handlers (3rd‑person)
    void LookUp(float Val);
    void Turn(float Val);

    // AI move callback
    void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

    // Apply or remove the speed cap
    void ApplySpeedLimit();

private:
    // — Patrol state —
    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    bool bIsPatrolMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Patrol", meta = (AllowPrivateAccess = "true"))
    int32 CurrentWPIndex = 0;

    TArray<AActor*> Waypoints;
    UPROPERTY()
    AAIController* AICon = nullptr;

    // stub for future treat detection
    UPROPERTY(BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
    int32 TreatsDetected = 0;

    // — Camera & view —
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* ThirdPersonCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* AerialCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    float CameraBlendTime = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Control", meta = (AllowPrivateAccess = "true"))
    float LookUpSpeed = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Control", meta = (AllowPrivateAccess = "true"))
    float TurnSpeed = 90.f;

    bool bUsingAerialView = false;
    void ChangeView();

    // — Lights —
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    USpotLightComponent* Headlight;

    UPROPERTY(BlueprintReadOnly, Category = "Lights", meta = (AllowPrivateAccess = "true"))
    bool bLightsOn = true;

    // — Speed limit —
    UPROPERTY(BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = "true"))
    bool bSpeedLimited = false;

    // — Parameters —
    UPROPERTY(EditAnywhere, Category = "Patrol")
    float AcceptanceRadius = 200.f;

    // — UI widget classes —
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> RobotStatsWidgetClass;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> PatrolInfoWidgetClass;
};