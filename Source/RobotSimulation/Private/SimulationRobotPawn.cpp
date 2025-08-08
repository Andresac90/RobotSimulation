#include "SimulationRobotPawn.h"
#include "PatrolVehicleMovementComponent.h"
#include "RobotAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PlayerController.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Waypoint.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Camera/PlayerCameraManager.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit.SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(
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
    AerialCamera->SetRelativeLocation({ 0,0,5000 });
    AerialCamera->SetRelativeRotation({ -90,0,0 });
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

    // show cursor & allow clicking UI (planning)
    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    }

    // Optional fallback: gather & sort level waypoints if used
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Waypoints);
    Waypoints.Sort([](const AActor& A, const AActor& B)
        {
            const AWaypoint* WA = Cast<AWaypoint>(&A);
            const AWaypoint* WB = Cast<AWaypoint>(&B);
            return (WA && WB) ? WA->PatrolOrder < WB->PatrolOrder : false;
        });

    // Enforce initial speed cap ON (your default was off)
    if (!bSpeedLimited) ToggleSpeedLimit();
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // update SpeedKmh
    SpeedKmh = 0.f;
    if (auto* M = GetVehicleMovementComponent())
    {
        SpeedKmh = M->GetForwardSpeed() * 0.036f;
    }

    // pick correct total count (dynamic checkpoints preferred)
    const int32 Total = (PatrolCheckpoints.Num() > 0) ? PatrolCheckpoints.Num() : Waypoints.Num();

    // push into UMG (blueprint event)
    OnUpdateHUD(
        SpeedKmh,
        bSpeedLimited,
        bLightsOn,
        bIsPatrolMode,
        TreatsDetected,
        Total > 0 ? (CurrentWPIndex + 1) : 0,
        Total
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
    // clamp throttle if speed-limit is on and we've hit/exceeded MaxSpeedKmh
    if (bSpeedLimited && SpeedKmh >= MaxSpeedKmh) Val = 0.f;

    if (!bIsPatrolMode)
        if (auto* M = Cast<UPatrolVehicleMovementComponent>(GetVehicleMovementComponent()))
            M->SetThrottleInput(Val);
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
    {
        SpringArm->AddLocalRotation(FRotator(Val * LookUpSpeed * GetWorld()->DeltaTimeSeconds, 0, 0));
    }
}

void ASimulationRobotPawn::Turn(float Val)
{
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(Val) > KINDA_SMALL_NUMBER)
    {
        SpringArm->AddLocalRotation(FRotator(0, Val * TurnSpeed * GetWorld()->DeltaTimeSeconds, 0));
    }
}

void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    ApplySpeedLimit();
}

void ASimulationRobotPawn::ApplySpeedLimit()
{
    // We clamp throttle in ThrottleInput() so MaxRPM stays snappy
    if (auto* C = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        C->EngineSetup.MaxRPM = 3000.f;
    }
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;

    if (bIsPatrolMode)
    {
        // → ENTER PATROL MODE (AI possesses)
        if (auto* PC = Cast<APlayerController>(GetController()))
        {
            PC->UnPossess();
        }

        if (!AICon)
        {
            // Spawn an AI controller (could also call SpawnDefaultController())
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AICon = GetWorld()->SpawnActor<ARobotAIController>(
                ARobotAIController::StaticClass(),
                GetActorLocation(), GetActorRotation(), Params);
        }

        if (AICon) AICon->Possess(this);

        if (AICon && AICon->GetPathFollowingComponent())
        {
            AICon->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
        }

        // Use dynamic checkpoints if available, otherwise fall back to waypoints
        if (PatrolCheckpoints.Num() > 0)
        {
            CurrentWPIndex = 0;
            AICon->MoveToLocation(PatrolCheckpoints[0], AcceptanceRadius);
        }
        else if (Waypoints.Num() > 0)
        {
            CurrentWPIndex = 0;
            AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
        }
    }
    else
    {
        // → EXIT PATROL MODE (Player possesses)
        if (AICon)
        {
            AICon->StopMovement();
            AICon->UnPossess();
        }

        if (auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            PC->Possess(this);
        }
    }
}

void ASimulationRobotPawn::BeginMission()
{
    if (!bIsPatrolMode) TogglePatrolMode();
}

void ASimulationRobotPawn::EndMission()
{
    if (bIsPatrolMode) TogglePatrolMode();
}

void ASimulationRobotPawn::ToggleLights()
{
    bLightsOn = !bLightsOn;
    if (Headlight) Headlight->SetVisibility(bLightsOn);
}

void ASimulationRobotPawn::ChangeView()
{
    bUsingAerialView = !bUsingAerialView;

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        if (ThirdPersonCamera) ThirdPersonCamera->SetActive(!bUsingAerialView);
        if (AerialCamera)      AerialCamera->SetActive(bUsingAerialView);

        FViewTargetTransitionParams Params;
        Params.BlendTime = CameraBlendTime;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(this, Params);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ChangeView(): no PlayerController"));
    }
}

void ASimulationRobotPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    AICon = Cast<AAIController>(NewController);
    if (AICon && AICon->GetPathFollowingComponent())
    {
        AICon->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
    }
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID, const FPathFollowingResult& Result)
{
    if (!Result.IsSuccess() || !bIsPatrolMode || !AICon) return;

    if (PatrolCheckpoints.Num() > 0)
    {
        CurrentWPIndex = (CurrentWPIndex + 1) % PatrolCheckpoints.Num();
        AICon->MoveToLocation(PatrolCheckpoints[CurrentWPIndex], AcceptanceRadius);
    }
    else if (Waypoints.Num() > 0)
    {
        CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
        AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
    }
}

// ------- New workflow helpers -------

void ASimulationRobotPawn::SetAerialView(bool bUseAerial)
{
    bUsingAerialView = bUseAerial;

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        if (ThirdPersonCamera) ThirdPersonCamera->SetActive(!bUsingAerialView);
        if (AerialCamera)      AerialCamera->SetActive(bUsingAerialView);

        FViewTargetTransitionParams Params;
        Params.BlendTime = CameraBlendTime;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(this, Params);
    }
}

void ASimulationRobotPawn::SetPatrolCheckpoints(const TArray<FVector>& CheckpointLocations)
{
    PatrolCheckpoints = CheckpointLocations;
    CurrentWPIndex = 0;

    UE_LOG(LogTemp, Log, TEXT("Set %d patrol checkpoints"), PatrolCheckpoints.Num());
}

bool ASimulationRobotPawn::ScreenToWorldLocation(FVector2D ScreenPosition, FVector& WorldLocation)
{
    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        FVector WorldDirection;
        if (PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldLocation, WorldDirection))
        {
            // Line trace to ground/geometry
            FHitResult Hit;
            const FVector Start = WorldLocation;
            const FVector End = Start + (WorldDirection * 100000.0f);

            FCollisionQueryParams Params(SCENE_QUERY_STAT(ScreenToWorld), /*bTraceComplex=*/false);
            Params.AddIgnoredActor(this);

            if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
            {
                WorldLocation = Hit.Location;
                return true;
            }

            // Fallback: plane Z=0
            const float DZ = WorldDirection.Z;
            if (FMath::Abs(DZ) > SMALL_NUMBER)
            {
                const float T = -Start.Z / DZ;
                if (T > 0.f)
                {
                    WorldLocation = Start + (WorldDirection * T);
                    return true;
                }
            }
        }
    }
    return false;
}
