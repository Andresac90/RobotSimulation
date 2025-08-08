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

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit.SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(
        AWheeledVehiclePawn::VehicleMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 500.f;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCam"));
    ThirdPersonCamera->SetupAttachment(SpringArm);

    AerialCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AerialCam"));
    AerialCamera->SetupAttachment(RootComponent);
    AerialCamera->SetRelativeLocation({ 0,0,5000 });
    AerialCamera->SetRelativeRotation({ -90,0,0 });
    AerialCamera->SetAutoActivate(false);

    Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
    Headlight->SetupAttachment(RootComponent);
    Headlight->Intensity = 5000.f;

    AutoPossessPlayer = EAutoReceiveInput::Player0;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ARobotAIController::StaticClass();
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    }

    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Waypoints);
    Waypoints.Sort([](const AActor& A, const AActor& B)
        {
            const AWaypoint* WA = Cast<AWaypoint>(&A);
            const AWaypoint* WB = Cast<AWaypoint>(&B);
            return (WA && WB) ? WA->PatrolOrder < WB->PatrolOrder : false;
        });

    if (!bSpeedLimited) ToggleSpeedLimit(); // start limited
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    SpeedKmh = 0.f;
    if (auto* M = GetVehicleMovementComponent())
        SpeedKmh = M->GetForwardSpeed() * 0.036f;

    const int32 Total = (PatrolCheckpoints.Num() > 0) ? PatrolCheckpoints.Num() : Waypoints.Num();
    OnUpdateHUD(SpeedKmh, bSpeedLimited, bLightsOn, bIsPatrolMode,
        TreatsDetected, Total > 0 ? CurrentWPIndex + 1 : 0, Total);
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* P)
{
    Super::SetupPlayerInputComponent(P);
    P->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    P->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    P->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);

    P->BindAction("RotateCamera", IE_Pressed, this, &ASimulationRobotPawn::StartCameraRotate);
    P->BindAction("RotateCamera", IE_Released, this, &ASimulationRobotPawn::StopCameraRotate);
    P->BindAxis("LookUp", this, &ASimulationRobotPawn::LookUp);
    P->BindAxis("Turn", this, &ASimulationRobotPawn::Turn);

    P->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
    P->BindAction("ToggleSpeed", IE_Pressed, this, &ASimulationRobotPawn::ToggleSpeedLimit);
    P->BindAction("ToggleLights", IE_Pressed, this, &ASimulationRobotPawn::ToggleLights);
    P->BindAction("ChangeView", IE_Pressed, this, &ASimulationRobotPawn::ChangeView);
}

void ASimulationRobotPawn::ThrottleInput(float Val)
{
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
void ASimulationRobotPawn::LookUp(float V) { if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER) SpringArm->AddLocalRotation({ V * LookUpSpeed * GetWorld()->DeltaTimeSeconds,0,0 }); }
void ASimulationRobotPawn::Turn(float V) { if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER) SpringArm->AddLocalRotation({ 0,V * TurnSpeed * GetWorld()->DeltaTimeSeconds,0 }); }

void ASimulationRobotPawn::ToggleSpeedLimit() { bSpeedLimited = !bSpeedLimited; ApplySpeedLimit(); }
void ASimulationRobotPawn::ApplySpeedLimit()
{
    if (auto* C = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        C->EngineSetup.MaxRPM = 3000.f; // keep response; throttle is clamped separately
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;

    if (bIsPatrolMode)
    {
        if (auto* PC = Cast<APlayerController>(GetController())) PC->UnPossess();

        if (!AICon)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AICon = GetWorld()->SpawnActor<ARobotAIController>(ARobotAIController::StaticClass(),
                GetActorLocation(), GetActorRotation(), Params);
        }

        if (AICon) AICon->Possess(this);

        if (AICon && AICon->GetPathFollowingComponent())
        {
            auto* PF = AICon->GetPathFollowingComponent();
            PF->OnRequestFinished.RemoveAll(this);                    // prevent duplicate binds
            PF->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
        }

        // initial move
        CurrentWPIndex = 0;
        if (PatrolCheckpoints.Num() > 0)
            AICon->MoveToLocation(PatrolCheckpoints[0], AcceptanceRadius);
        else if (Waypoints.Num() > 0)
            AICon->MoveToActor(Waypoints[0], AcceptanceRadius);
    }
    else
    {
        if (AICon)
        {
            if (auto* PF = AICon->GetPathFollowingComponent())
                PF->OnRequestFinished.RemoveAll(this);
            AICon->StopMovement();
            AICon->UnPossess();
        }

        if (auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            PC->Possess(this);
    }
}

void ASimulationRobotPawn::BeginMission() { if (!bIsPatrolMode) TogglePatrolMode(); }
void ASimulationRobotPawn::EndMission() { if (bIsPatrolMode) TogglePatrolMode(); }

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

        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(this, Params);
    }
}

void ASimulationRobotPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    AICon = Cast<AAIController>(NewController);
    if (AICon && AICon->GetPathFollowingComponent())
    {
        auto* PF = AICon->GetPathFollowingComponent();
        PF->OnRequestFinished.RemoveAll(this);
        PF->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
    }
}

void ASimulationRobotPawn::OnMoveCompleted(FAIRequestID, const FPathFollowingResult& Result)
{
    if (!Result.IsSuccess() || !bIsPatrolMode || !AICon) return;
    AdvanceToNextPatrolTarget();
}

void ASimulationRobotPawn::AdvanceToNextPatrolTarget()
{
    // Safety: if AI was destroyed/unpossessed between callbacks
    if (!AICon) return;

    const bool bUsingDynamic = (PatrolCheckpoints.Num() > 0);
    const int32 Count = bUsingDynamic ? PatrolCheckpoints.Num() : Waypoints.Num();
    if (Count <= 0) return;

    // Next index in a loop
    const int32 NextIndex = (CurrentWPIndex + 1) % Count;

    // Guard: if only one point, do not re-issue a MoveTo (prevents tight completion loop)
    if (NextIndex == CurrentWPIndex) return;

    CurrentWPIndex = NextIndex;

    if (bUsingDynamic)
    {
        AICon->MoveToLocation(PatrolCheckpoints[CurrentWPIndex], AcceptanceRadius);
    }
    else
    {
        AICon->MoveToActor(Waypoints[CurrentWPIndex], AcceptanceRadius);
    }
}


void ASimulationRobotPawn::SetAerialView(bool bUseAerial)
{
    bUsingAerialView = bUseAerial;

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        if (ThirdPersonCamera) ThirdPersonCamera->SetActive(!bUsingAerialView);
        if (AerialCamera)      AerialCamera->SetActive(bUsingAerialView);

        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
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
        FVector WorldDir;
        if (PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldLocation, WorldDir))
        {
            FHitResult Hit;
            const FVector Start = WorldLocation;
            const FVector End = Start + (WorldDir * 100000.0f);

            FCollisionQueryParams Params(SCENE_QUERY_STAT(ScreenToWorld), false);
            Params.AddIgnoredActor(this);

            if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
            {
                WorldLocation = Hit.Location;
                return true;
            }

            if (!FMath::IsNearlyZero(WorldDir.Z))
            {
                const float T = -Start.Z / WorldDir.Z;
                if (T > 0.f) { WorldLocation = Start + (WorldDir * T); return true; }
            }
        }
    }
    return false;
}
