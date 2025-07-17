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

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit
        .SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(
            AWheeledVehiclePawn::VehicleMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    // Spring arm + third‑person camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 500.f;
    SpringArm->bUsePawnControlRotation = false;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCam"));
    ThirdPersonCamera->SetupAttachment(SpringArm);
    ThirdPersonCamera->bUsePawnControlRotation = false;

    // Aerial camera, start deactivated
    AerialCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AerialCam"));
    AerialCamera->SetupAttachment(RootComponent);
    AerialCamera->SetRelativeLocation(FVector(0, 0, 1500));
    AerialCamera->SetAutoActivate(false);

    // Headlight
    Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
    Headlight->SetupAttachment(RootComponent);
    Headlight->SetIntensity(5000.f);
    bLightsOn = true;

    // Input possession
    AutoPossessPlayer = EAutoReceiveInput::Player0;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ARobotAIController::StaticClass();
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    // Gather & sort waypoints
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Waypoints);
    Waypoints.Sort([](const AActor& A, const AActor& B) {
        const AWaypoint* WaypointA = Cast<AWaypoint>(&A);
        const AWaypoint* WaypointB = Cast<AWaypoint>(&B);
        if (WaypointA && WaypointB)
        {
            return WaypointA->PatrolOrder < WaypointB->PatrolOrder;
        }
        return false;
        });

    // Spawn UI panels
    if (RobotStatsWidgetClass)
    {
        UUserWidget* StatsWidget = CreateWidget<UUserWidget>(GetWorld(), RobotStatsWidgetClass);
        if (StatsWidget)
        {
            StatsWidget->AddToViewport();
        }
    }

    if (PatrolInfoWidgetClass)
    {
        UUserWidget* PatrolWidget = CreateWidget<UUserWidget>(GetWorld(), PatrolInfoWidgetClass);
        if (PatrolWidget)
        {
            PatrolWidget->AddToViewport();
        }
    }

    // Start with speed limit on
    ToggleSpeedLimit();
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Compute speed & push to HUD
    float SpeedCms = 0.f;
    if (GetVehicleMovementComponent())
    {
        SpeedCms = GetVehicleMovementComponent()->GetForwardSpeed();
    }
    float SpeedKmh = SpeedCms * 0.036f;

    OnUpdateHUD(
        SpeedKmh,
        bSpeedLimited,
        bLightsOn,
        bIsPatrolMode,
        TreatsDetected,
        CurrentWPIndex + 1,    // display as 1‑based
        Waypoints.Num()
    );
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Driving
    PlayerInputComponent->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    PlayerInputComponent->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    PlayerInputComponent->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);

    // Looking (3rd‑person)
    PlayerInputComponent->BindAxis("LookUp", this, &ASimulationRobotPawn::LookUp);
    PlayerInputComponent->BindAxis("Turn", this, &ASimulationRobotPawn::Turn);

    // Actions
    PlayerInputComponent->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
    PlayerInputComponent->BindAction("ToggleSpeed", IE_Pressed, this, &ASimulationRobotPawn::ToggleSpeedLimit);
    PlayerInputComponent->BindAction("ToggleLights", IE_Pressed, this, &ASimulationRobotPawn::ToggleLights);
    PlayerInputComponent->BindAction("ChangeView", IE_Pressed, this, &ASimulationRobotPawn::ChangeView);
}

void ASimulationRobotPawn::ThrottleInput(float Val)
{
    if (!bIsPatrolMode)
    {
        UPatrolVehicleMovementComponent* PatrolMovement = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent());
        if (PatrolMovement)
        {
            PatrolMovement->SetThrottleInput(Val);
        }
    }
}

void ASimulationRobotPawn::SteeringInput(float Val)
{
    if (!bIsPatrolMode)
    {
        UPatrolVehicleMovementComponent* PatrolMovement = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent());
        if (PatrolMovement)
        {
            PatrolMovement->SetSteeringInput(Val);
        }
    }
}

void ASimulationRobotPawn::HandbrakeInput(float Val)
{
    UPatrolVehicleMovementComponent* PatrolMovement = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent());
    if (PatrolMovement)
    {
        PatrolMovement->SetHandbrakeInput(Val > KINDA_SMALL_NUMBER);
    }
}

void ASimulationRobotPawn::LookUp(float Val)
{
    if (!bUsingAerialView && FMath::Abs(Val) > KINDA_SMALL_NUMBER && SpringArm)
    {
        SpringArm->AddRelativeRotation(FRotator(Val * LookUpSpeed * GetWorld()->GetDeltaSeconds(), 0, 0));
    }
}

void ASimulationRobotPawn::Turn(float Val)
{
    if (!bUsingAerialView && FMath::Abs(Val) > KINDA_SMALL_NUMBER && SpringArm)
    {
        SpringArm->AddRelativeRotation(FRotator(0, Val * TurnSpeed * GetWorld()->GetDeltaSeconds(), 0));
    }
}

void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    ApplySpeedLimit();
}

void ASimulationRobotPawn::ApplySpeedLimit()
{
    UChaosWheeledVehicleMovementComponent* ChaosMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
    if (ChaosMovement)
    {
        ChaosMovement->EngineSetup.MaxRPM = bSpeedLimited ? 500.f : 3000.f;
        ChaosMovement->EngineSetup.TorqueCurve.EditorCurveData.Reset();
        ChaosMovement->EngineSetup.TorqueCurve.EditorCurveData.AddKey(0.f, bSpeedLimited ? 150.f : 260.f);
        ChaosMovement->EngineSetup.TorqueCurve.EditorCurveData.AddKey(bSpeedLimited ? 500.f : 3000.f, bSpeedLimited ? 150.f : 260.f);
    }
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;

    if (!AICon)
    {
        AICon = Cast<AAIController>(GetController());
    }

    if (bIsPatrolMode && Waypoints.Num() > 0)
    {
        CurrentWPIndex = 0;
        if (AICon && Waypoints.IsValidIndex(0))
        {
            AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
        }
    }
    else if (AICon)
    {
        AICon->StopMovement();
    }
}

void ASimulationRobotPawn::BeginMission()
{
    if (!bIsPatrolMode)
    {
        TogglePatrolMode();
    }
}

void ASimulationRobotPawn::EndMission()
{
    if (bIsPatrolMode)
    {
        TogglePatrolMode();
    }
}

void ASimulationRobotPawn::ToggleLights()
{
    bLightsOn = !bLightsOn;
    if (Headlight)
    {
        Headlight->SetVisibility(bLightsOn);
    }
}

void ASimulationRobotPawn::ChangeView()
{
    bUsingAerialView = !bUsingAerialView;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        AActor* TargetActor = bUsingAerialView ? AerialCamera->GetOwner() : ThirdPersonCamera->GetOwner();
        if (TargetActor)
        {
            PC->SetViewTargetWithBlend(TargetActor, CameraBlendTime);
        }
    }
}

void ASimulationRobotPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    AICon = Cast<AAIController>(NewController);
    if (AICon)
    {
        UPathFollowingComponent* PathFollowing = AICon->GetPathFollowingComponent();
        if (PathFollowing)
        {
            PathFollowing->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
        }
    }
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    if (!Result.IsSuccess() || !bIsPatrolMode || !AICon || Waypoints.Num() == 0)
    {
        return;
    }

    CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
    if (Waypoints.IsValidIndex(CurrentWPIndex))
    {
        AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
    }
}