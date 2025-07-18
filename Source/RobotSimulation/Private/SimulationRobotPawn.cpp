// SimulationRobotPawn.cpp
#include "SimulationRobotPawn.h"
#include "PatrolVehicleMovementComponent.h"
#include "RobotAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PlayerController.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Waypoint.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Camera/PlayerCameraManager.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit
        .SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(
            AWheeledVehiclePawn::VehicleMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    // --- Cameras & spring arm (editable in BP) ---
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 500.f;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bEditableWhenInherited = true;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCam"));
    ThirdPersonCamera->SetupAttachment(SpringArm);
    ThirdPersonCamera->bUsePawnControlRotation = false;
    ThirdPersonCamera->bEditableWhenInherited = true;

    AerialCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AerialCam"));
    AerialCamera->SetupAttachment(RootComponent);
    AerialCamera->SetRelativeLocation(FVector(0, 0, 1500));
    AerialCamera->SetAutoActivate(false);
    AerialCamera->bEditableWhenInherited = true;

    // Headlight
    Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
    Headlight->SetupAttachment(RootComponent);
    Headlight->Intensity = 5000.f;
    bLightsOn = true;

    // Possession
    AutoPossessPlayer = EAutoReceiveInput::Player0;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ARobotAIController::StaticClass();
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    // show cursor & allow clicking UI
    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI()
            .SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    }

    // gather & sort waypoints
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Waypoints);
    Waypoints.Sort([](const AActor& A, const AActor& B) {
        auto* WA = Cast<AWaypoint>(&A);
        auto* WB = Cast<AWaypoint>(&B);
        return WA && WB ? WA->PatrolOrder < WB->PatrolOrder : false;
        });

    // spawn UI panels
    if (RobotStatsWidgetClass)
        if (auto* W = CreateWidget<UUserWidget>(GetWorld(), RobotStatsWidgetClass))
            W->AddToViewport();

    if (PatrolInfoWidgetClass)
        if (auto* W = CreateWidget<UUserWidget>(GetWorld(), PatrolInfoWidgetClass))
            W->AddToViewport();

    // enforce initial speed cap
    ToggleSpeedLimit();
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // update SpeedKmh
    SpeedKmh = 0.f;
    if (auto* M = GetVehicleMovementComponent())
        SpeedKmh = M->GetForwardSpeed() * 0.036f;

    // push into UMG
    OnUpdateHUD(
        SpeedKmh,
        bSpeedLimited,
        bLightsOn,
        bIsPatrolMode,
        TreatsDetected,
        CurrentWPIndex + 1,
        Waypoints.Num()
    );
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* P)
{
    Super::SetupPlayerInputComponent(P);

    // driving
    P->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    P->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    P->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);

    // rotate camera on LMB + mouse
    P->BindAction("RotateCamera", IE_Pressed, this, &ASimulationRobotPawn::StartCameraRotate);
    P->BindAction("RotateCamera", IE_Released, this, &ASimulationRobotPawn::StopCameraRotate);
    P->BindAxis("LookUp", this, &ASimulationRobotPawn::LookUp);
    P->BindAxis("Turn", this, &ASimulationRobotPawn::Turn);

    // other inputs
    P->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
    P->BindAction("ToggleSpeed", IE_Pressed, this, &ASimulationRobotPawn::ToggleSpeedLimit);
    P->BindAction("ToggleLights", IE_Pressed, this, &ASimulationRobotPawn::ToggleLights);
    P->BindAction("ChangeView", IE_Pressed, this, &ASimulationRobotPawn::ChangeView);
}

void ASimulationRobotPawn::ThrottleInput(float Val)
{
    // clamp throttle if speed‑limit is on and we've hit/exceeded MaxSpeedKmh
    if (bSpeedLimited && SpeedKmh >= MaxSpeedKmh)
    {
        Val = 0.f;
    }

    if (!bIsPatrolMode)
    {
        if (auto* M = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent()))
            M->SetThrottleInput(Val);
    }
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    if (!bIsPatrolMode)
        if (auto* M = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent()))
            M->SetSteeringInput(Val);
}

void ASimulationRobotPawn::HandbrakeInput(float Val)
{
    if (auto* M = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent()))
        M->SetHandbrakeInput(Val > KINDA_SMALL_NUMBER);
}

void ASimulationRobotPawn::StartCameraRotate() { bRotatingCamera = true; }
void ASimulationRobotPawn::StopCameraRotate() { bRotatingCamera = false; }

void ASimulationRobotPawn::LookUp(float Val)
{
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(Val * LookUpSpeed * GetWorld()->DeltaTimeSeconds, 0, 0));
}

void ASimulationRobotPawn::Turn(float Val)
{
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(0, Val * TurnSpeed * GetWorld()->DeltaTimeSeconds, 0));
}

void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    ApplySpeedLimit();
}

void ASimulationRobotPawn::ApplySpeedLimit()
{
    // we leave the RPM curve alone now — throttle clamping does the real cap
    if (auto* C = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        C->EngineSetup.MaxRPM = bSpeedLimited ? 3000.f : 3000.f;
        // (optional) you can still tweak torque‐curve here if you like
    }
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;
    if (!AICon) AICon = Cast<AAIController>(GetController());

    if (bIsPatrolMode && Waypoints.Num())
    {
        CurrentWPIndex = 0;
        AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
    }
    else if (AICon)
    {
        AICon->StopMovement();
    }
}

void ASimulationRobotPawn::BeginMission() { if (!bIsPatrolMode) TogglePatrolMode(); }
void ASimulationRobotPawn::EndMission() { if (bIsPatrolMode)  TogglePatrolMode(); }

void ASimulationRobotPawn::ToggleLights()
{
    bLightsOn = !bLightsOn;
    if (Headlight) Headlight->SetVisibility(bLightsOn);
}

void ASimulationRobotPawn::ChangeView()
{
    bUsingAerialView = !bUsingAerialView;

    auto* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("ChangeView(): no PlayerController"));
        return;
    }

    // cameras should now always be valid
    ThirdPersonCamera->SetActive(!bUsingAerialView);
    AerialCamera->SetActive(bUsingAerialView);

    FViewTargetTransitionParams Params;
    Params.BlendTime = CameraBlendTime;
    Params.BlendFunction = VTBlend_Cubic;
    PC->SetViewTarget(this, Params);
}

void ASimulationRobotPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    AICon = Cast<AAIController>(NewController);
    if (AICon && AICon->GetPathFollowingComponent())
    {
        AICon->GetPathFollowingComponent()
            ->OnRequestFinished
            .AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
    }
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID, const FPathFollowingResult& Result)
{
    if (!Result.IsSuccess() || !bIsPatrolMode || !AICon || Waypoints.Num() == 0)
        return;

    CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
    AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
}
