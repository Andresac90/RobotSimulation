#include "SimulationRobotPawn.h"
#include "GM_Simulation.h"
#include "PatrolVehicleMovementComponent.h"
#include "RobotAIController.h"

#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "Camera/CameraActor.h"
#include "Waypoint.h"

#include "NavigationSystem.h"
#include "NavigationPath.h"

#include "DrawDebugHelpers.h"
#include "Components/InputComponent.h"
#include "ThreatComponent.h"
#include "ThreatBoxesWidget.h"
#include "ThreatScreenBox.h"

#define LOG_PTR(Name, Ptr) UE_LOG(LogTemp, Log, TEXT("[RobotPawn] %s: %s"), TEXT(Name), Ptr ? *Ptr->GetName() : TEXT("<null>"))

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit.SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    // Spring arm + 3P camera (farther + smooth)
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = ThirdPersonArmLength;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bEnableCameraLag = bEnableCamLag;
    SpringArm->CameraLagSpeed = CamLagSpeed;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCam"));
    ThirdPersonCamera->SetupAttachment(SpringArm);
    ThirdPersonCamera->bUsePawnControlRotation = false;

    // Interior camera (yaw only via InteriorPivot)
    InteriorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("InteriorPivot"));
    InteriorPivot->SetupAttachment(RootComponent);
    InteriorPivot->SetRelativeLocation(FVector(0, 0, 120.f)); // adjust to car cabin
    InteriorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteriorCam"));
    InteriorCamera->SetupAttachment(InteriorPivot);
    InteriorCamera->bUsePawnControlRotation = false;
    InteriorCamera->SetAutoActivate(false);

    // Aerial camera
    AerialCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AerialCam"));
    AerialCamera->SetupAttachment(RootComponent);
    AerialCamera->SetRelativeLocation(FVector(0, 0, 5000));
    AerialCamera->SetRelativeRotation(FRotator(-90, 0, 0));
    AerialCamera->SetAutoActivate(false);

    // Two headlights
    HeadlightLeft = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightLeft"));
    HeadlightRight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightRight"));
    HeadlightLeft->SetupAttachment(RootComponent);
    HeadlightRight->SetupAttachment(RootComponent);
    HeadlightLeft->SetRelativeLocation(FVector(120.f, -40.f, 40.f));
    HeadlightRight->SetRelativeLocation(FVector(120.f, 40.f, 40.f));
    HeadlightLeft->Intensity = 5000.f;
    HeadlightRight->Intensity = 5000.f;

    // Threat sensor sphere
    ThreatSensor = CreateDefaultSubobject<USphereComponent>(TEXT("ThreatSensor"));
    ThreatSensor->SetupAttachment(RootComponent);
    ThreatSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ThreatSensor->SetCollisionObjectType(ECC_WorldDynamic);
    ThreatSensor->SetCollisionResponseToAllChannels(ECR_Overlap);
    ThreatSensor->SetSphereRadius(ThreatSenseRadius);

    // Player controls by default; AI only when patrolling
    AutoPossessPlayer = EAutoReceiveInput::Player0;
    AutoPossessAI = EAutoPossessAI::Disabled;
    AIControllerClass = ARobotAIController::StaticClass();
}

void ASimulationRobotPawn::ResolveCriticalComponents()
{
    if (!SpringArm)
    {
        SpringArm = FindComponentByClass<USpringArmComponent>();
        if (SpringArm)
        {
            SpringArm->bUsePawnControlRotation = false;
            SpringArm->TargetArmLength = ThirdPersonArmLength;
            SpringArm->bEnableCameraLag = bEnableCamLag;
            SpringArm->CameraLagSpeed = CamLagSpeed;
        }
    }
    if (!ThirdPersonCamera) ThirdPersonCamera = FindComponentByClass<UCameraComponent>();

    if (!AerialCamera)
    {
        TArray<UCameraComponent*> Cams; GetComponents<UCameraComponent>(Cams);
        for (UCameraComponent* C : Cams) { if (C && C != ThirdPersonCamera) { AerialCamera = C; break; } }
    }

    if (!HeadlightLeft)  HeadlightLeft = FindComponentByClass<USpotLightComponent>();
    if (!HeadlightRight) HeadlightRight = HeadlightLeft; // fallback: single light setups
    if (!ThreatSensor)   ThreatSensor = FindComponentByClass<USphereComponent>();
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    ResolveCriticalComponents();
    LOG_PTR("SpringArm", SpringArm);
    LOG_PTR("ThirdPersonCamera", ThirdPersonCamera);
    LOG_PTR("AerialCamera", AerialCamera);
    LOG_PTR("HeadlightLeft", HeadlightLeft);
    LOG_PTR("HeadlightRight", HeadlightRight);
    LOG_PTR("ThreatSensor", ThreatSensor);

    if (SpringArm)
    {
        SpringArm->TargetArmLength = ThirdPersonArmLength;
        SpringArm->bEnableCameraLag = bEnableCamLag;
        SpringArm->CameraLagSpeed = CamLagSpeed;
    }

    if (AGM_Simulation* GM = GetWorld()->GetAuthGameMode<AGM_Simulation>())
        GM->NotifyRobotReady(this);

    ForceThirdPersonCamera();
    EnsureViewTargetProxy();

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (PC)
    {
        ApplyAlwaysInteractiveInput(PC); // mouse always ON; keys & clicks active

        if (ThreatOverlayWidgetClass)
        {
            ThreatOverlayWidget = CreateWidget<UThreatBoxesWidget>(PC, ThreatOverlayWidgetClass);
            if (ThreatOverlayWidget) ThreatOverlayWidget->AddToViewport(50);
        }
    }

    // Designer waypoints
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Waypoints);
    Waypoints.Sort([](const AActor& A, const AActor& B)
        {
            const AWaypoint* WA = Cast<AWaypoint>(&A);
            const AWaypoint* WB = Cast<AWaypoint>(&B);
            return (WA && WB) ? WA->PatrolOrder < WB->PatrolOrder : false;
        });

    if (!bSpeedLimited) ToggleSpeedLimit();

    if (ThreatSensor)
    {
        ThreatSensor->OnComponentBeginOverlap.AddDynamic(this, &ASimulationRobotPawn::OnThreatBegin);
        ThreatSensor->OnComponentEndOverlap.AddDynamic(this, &ASimulationRobotPawn::OnThreatEnd);
    }
}

void ASimulationRobotPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AGM_Simulation* GM = GetWorld()->GetAuthGameMode<AGM_Simulation>())
        if (GM->GetRobotPawn() == this) GM->NotifyRobotReady(nullptr);

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        RemoveAuxInput(PC);

    Super::EndPlay(EndPlayReason);
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    SpeedKmh = 0.f;
    if (auto* M = GetVehicleMovementComponent())
        SpeedKmh = M->GetForwardSpeed() * 0.036f;

    if (bDrawThreatBoxes) DrawThreatDebug();
    UpdateThreatOverlay();

    const int32 Total = (PatrolCheckpoints.Num() > 0) ? PatrolCheckpoints.Num() : Waypoints.Num();
    OnUpdateHUD(SpeedKmh, bSpeedLimited, bLightsOn, bIsPatrolMode,
        NearbyThreats.Num(), Total > 0 ? CurrentWPIndex + 1 : 0, Total);
}

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* P)
{
    Super::SetupPlayerInputComponent(P);

    BindCommonInputs(P);

    UE_LOG(LogTemp, Log, TEXT("[RobotPawn] Input bindings set on main InputComponent."));
}

void ASimulationRobotPawn::BindCommonInputs(UInputComponent* IC)
{
    if (!IC) return;

    // Axis
    IC->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    IC->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    IC->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);
    IC->BindAxis("LookUp", this, &ASimulationRobotPawn::LookUp);
    IC->BindAxis("Turn", this, &ASimulationRobotPawn::Turn);

    // Actions
    IC->BindAction("RotateCamera", IE_Pressed, this, &ASimulationRobotPawn::StartCameraRotate);
    IC->BindAction("RotateCamera", IE_Released, this, &ASimulationRobotPawn::StopCameraRotate);
    IC->BindAction("TogglePatrol", IE_Pressed, this, &ASimulationRobotPawn::TogglePatrolMode);
    IC->BindAction("ToggleSpeed", IE_Pressed, this, &ASimulationRobotPawn::ToggleSpeedLimit);
    IC->BindAction("ToggleLights", IE_Pressed, this, &ASimulationRobotPawn::ToggleLights);
    IC->BindAction("ChangeView", IE_Pressed, this, &ASimulationRobotPawn::ChangeView);
    IC->BindAction("ToggleVehicleCamera", IE_Pressed, this, &ASimulationRobotPawn::ToggleVehicleCamera);
}

void ASimulationRobotPawn::InstallAuxInput(APlayerController* PC)
{
    if (!PC || AuxInput) return;

    AuxInput = NewObject<UInputComponent>(this, TEXT("AuxInputComponent"));
    AuxInput->bBlockInput = false;
    AuxInput->Priority = 10; // above default
    AuxInput->RegisterComponent();
    BindCommonInputs(AuxInput);
    PC->PushInputComponent(AuxInput);

    UE_LOG(LogTemp, Log, TEXT("[RobotPawn] Aux input installed (keys remain active while AI possesses)."));
}

void ASimulationRobotPawn::RemoveAuxInput(APlayerController* PC)
{
    if (!PC || !AuxInput) return;
    PC->PopInputComponent(AuxInput);
    AuxInput->DestroyComponent();
    AuxInput = nullptr;
}

void ASimulationRobotPawn::ApplyAlwaysInteractiveInput(APlayerController* PC)
{
    if (!PC) return;

    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetWidgetToFocus(nullptr); // keep focus on viewport so keys reach us
    PC->SetInputMode(Mode);

    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    PC->bEnableMouseOverEvents = true;
}

// ----------------- input helpers -----------------
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

void ASimulationRobotPawn::LookUp(float V)
{
    if (bUsingInteriorView)
    {
        // interior: no vertical look (horizontal only)
        return;
    }

    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(V * LookUpSpeed * GetWorld()->DeltaTimeSeconds, 0, 0));
}

void ASimulationRobotPawn::Turn(float V)
{
    if (bUsingInteriorView)
    {
        if (InteriorPivot && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        {
            const float YawDelta = V * InteriorTurnSpeed * GetWorld()->DeltaTimeSeconds;
            FRotator R = InteriorPivot->GetRelativeRotation();
            R.Yaw = FMath::Fmod(R.Yaw + YawDelta, 360.f);
            InteriorPivot->SetRelativeRotation(R);
        }
        return;
    }

    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(0, V * TurnSpeed * GetWorld()->DeltaTimeSeconds, 0));
}

// ----------------- modes -----------------
void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    ApplySpeedLimit();
}

void ASimulationRobotPawn::ApplySpeedLimit()
{
    if (auto* C = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        C->EngineSetup.MaxRPM = 3000.f; // gentle cap; braking handled in movement component
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    UE_LOG(LogTemp, Log, TEXT("[RobotPawn] TogglePatrolMode pressed. Current bIsPatrolMode=%s"), bIsPatrolMode ? TEXT("true") : TEXT("false"));

    bIsPatrolMode = !bIsPatrolMode;

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (bIsPatrolMode)
    {
        if (APlayerController* CurrPC = Cast<APlayerController>(GetController()))
            CurrPC->UnPossess();

        if (!AICon)
        {
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AICon = GetWorld()->SpawnActor<ARobotAIController>(ARobotAIController::StaticClass(), GetActorLocation(), GetActorRotation(), Params);
        }
        if (AICon) AICon->Possess(this);

        if (AICon && AICon->GetPathFollowingComponent())
        {
            auto* PF = AICon->GetPathFollowingComponent();
            PF->OnRequestFinished.RemoveAll(this);
            PF->OnRequestFinished.AddUObject(this, &ASimulationRobotPawn::OnMoveCompleted);
        }

        CurrentWPIndex = 0;

        if (PatrolCheckpoints.Num() > 0)
            BuildAndFollowCenterPathTo(PatrolCheckpoints[CurrentWPIndex]);
        else if (Waypoints.Num() > 0)
            BuildAndFollowCenterPathTo(Waypoints[CurrentWPIndex]->GetActorLocation());
        else
            UE_LOG(LogTemp, Warning, TEXT("[RobotPawn] No checkpoints/waypoints to patrol."));

        if (PC)
        {
            InstallAuxInput(PC);          // keep hotkeys alive
            ApplyAlwaysInteractiveInput(PC); // mouse visible, clicks enabled during sim
        }
    }
    else
    {
        ActivePathPoints.Reset();
        ActivePathIndex = INDEX_NONE;
        bFollowingSubPath = false;

        if (AICon)
        {
            if (auto* PF = AICon->GetPathFollowingComponent()) PF->OnRequestFinished.RemoveAll(this);
            AICon->StopMovement(); AICon->UnPossess();
        }

        if (PC)
        {
            RemoveAuxInput(PC);
            PC->Possess(this);
            ApplyAlwaysInteractiveInput(PC); // still want mouse visible while editing
        }
    }
}

void ASimulationRobotPawn::BeginMission() { if (!bIsPatrolMode) TogglePatrolMode(); }
void ASimulationRobotPawn::EndMission() { if (bIsPatrolMode) TogglePatrolMode(); }

void ASimulationRobotPawn::ToggleLights()
{
    bLightsOn = !bLightsOn;
    if (HeadlightLeft)  HeadlightLeft->SetVisibility(bLightsOn);
    if (HeadlightRight) HeadlightRight->SetVisibility(bLightsOn);
}

void ASimulationRobotPawn::ToggleVehicleCamera()
{
    // Toggle between 3rd-person and interior (independent from aerial)
    bUsingInteriorView = !bUsingInteriorView;

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;

        if (bUsingInteriorView && InteriorCamera)
        {
            InteriorCamera->SetActive(true);
            ThirdPersonCamera->SetActive(false);
            AerialCamera->SetActive(false);
            PC->SetViewTarget(this, Params); // pawn calc camera picks active interior cam
        }
        else
        {
            InteriorCamera->SetActive(false);
            ThirdPersonCamera->SetActive(true);
            if (bUsingAerialView) AerialCamera->SetActive(false);
            EnsureViewTargetProxy();
            PC->SetViewTarget(ViewTargetProxy ? ViewTargetProxy : (AActor*)this, Params);
        }
    }
}

void ASimulationRobotPawn::ChangeView()
{
    // 3rd-person <-> Aerial (interior is separate toggle)
    bUsingAerialView = !bUsingAerialView;
    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
        if (bUsingAerialView && AerialCamera)
        {
            AerialCamera->SetActive(true);
            ThirdPersonCamera->SetActive(false);
            InteriorCamera->SetActive(false);
            PC->SetViewTarget(this, Params);
        }
        else
        {
            ThirdPersonCamera->SetActive(true);
            AerialCamera->SetActive(false);
            InteriorCamera->SetActive(false);
            EnsureViewTargetProxy();
            if (ViewTargetProxy) PC->SetViewTarget(ViewTargetProxy, Params);
        }
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
    if (bFollowingSubPath && AICon)
    {
        if (Result.IsSuccess())
        {
            if (ActivePathIndex != INDEX_NONE && ActivePathIndex + 1 < ActivePathPoints.Num())
            {
                ActivePathIndex++;
                FollowNextSubPoint();
                return;
            }
        }
        bFollowingSubPath = false;
        ActivePathPoints.Reset();
        ActivePathIndex = INDEX_NONE;
    }

    if (!bIsPatrolMode || !AICon) return;
    AdvanceToNextPatrolTarget();
}

void ASimulationRobotPawn::AdvanceToNextPatrolTarget()
{
    if (!AICon) return;

    const bool  bDynamic = (PatrolCheckpoints.Num() > 0);
    const int32 Count = bDynamic ? PatrolCheckpoints.Num() : Waypoints.Num();
    if (Count <= 0) return;

    const int32 Next = (CurrentWPIndex + 1) % Count;
    if (Next == CurrentWPIndex) return;

    CurrentWPIndex = Next;

    if (PatrolCheckpoints.Num() > 0)
        BuildAndFollowCenterPathTo(PatrolCheckpoints[CurrentWPIndex]);
    else if (Waypoints.Num() > 0)
        BuildAndFollowCenterPathTo(Waypoints[CurrentWPIndex]->GetActorLocation());
}

void ASimulationRobotPawn::IssueMoveToCurrentTarget()
{
    if (!AICon) return;

    FAIMoveRequest Req;
    Req.SetUsePathfinding(true);
    Req.SetAllowPartialPath(false);
    Req.SetAcceptanceRadius(AcceptanceRadius);

    if (PatrolCheckpoints.Num() > 0)      Req.SetGoalLocation(PatrolCheckpoints[CurrentWPIndex]);
    else if (Waypoints.Num() > 0)         Req.SetGoalActor(Waypoints[CurrentWPIndex]);

    AICon->MoveTo(Req);
}

// ----- path smoothing & follow -----
static FVector ClosestPointOnSegment(const FVector& A, const FVector& B, const FVector& P)
{
    const FVector AB = B - A;
    const float AB2 = AB.SizeSquared();
    if (AB2 <= KINDA_SMALL_NUMBER) return A;
    const float T = FMath::Clamp(FVector::DotProduct(P - A, AB) / AB2, 0.f, 1.f);
    return A + AB * T;
}

void ASimulationRobotPawn::SmoothCorners(const TArray<FVector>& InPoints, TArray<FVector>& Out) const
{
    Out.Reset();
    if (InPoints.Num() <= 2)
    {
        Out = InPoints;
        return;
    }

    Out.Add(InPoints[0]);

    for (int32 i = 1; i < InPoints.Num() - 1; ++i)
    {
        const FVector Prev = InPoints[i - 1];
        const FVector Curr = InPoints[i];
        const FVector Next = InPoints[i + 1];

        FVector InDir = (Curr - Prev);  float InLen = InDir.Size();  InDir = (InLen > 1.f) ? InDir / InLen : FVector::ForwardVector;
        FVector OutDir = (Next - Curr); float OutLen = OutDir.Size(); OutDir = (OutLen > 1.f) ? OutDir / OutLen : FVector::ForwardVector;

        const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(InDir, OutDir), -1.f, 1.f)));
        if (AngleDeg < MinCornerAngleDeg || (InLen < 1.f) || (OutLen < 1.f))
        {
            Out.Add(Curr);
            continue;
        }

        const float R = FMath::Min3(CornerRadius, InLen * 0.45f, OutLen * 0.45f);
        const FVector A = Curr - InDir * R; // before corner
        const FVector B = Curr + OutDir * R; // after corner

        Out.Add(A);
        Out.Add(ClosestPointOnSegment(A, B, Curr + (InDir + OutDir).GetSafeNormal() * (R * 0.5f)));
        Out.Add(B);
    }

    Out.Add(InPoints.Last());
}

void ASimulationRobotPawn::BuildAndFollowCenterPathTo(const FVector& GoalLocation)
{
    ActivePathPoints.Reset();
    ActivePathIndex = INDEX_NONE;
    bFollowingSubPath = false;

    if (!AICon) return;

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys) { IssueMoveToCurrentTarget(); return; }

    // Project start & goal to navmesh
    FNavLocation StartProj, GoalProj;
    const FVector Extent(300.f, 300.f, 600.f);
    const FVector StartLoc = GetActorLocation();
    bool bStartOk = NavSys->ProjectPointToNavigation(StartLoc, StartProj, Extent);
    bool bGoalOk = NavSys->ProjectPointToNavigation(GoalLocation, GoalProj, Extent);
    if (!bStartOk || !bGoalOk)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RobotPawn] ProjectPointToNavigation failed (start:%d goal:%d)"), bStartOk, bGoalOk);
        IssueMoveToCurrentTarget();
        return;
    }

    UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), StartProj.Location, GoalProj.Location, this);
    if (!Path || Path->PathPoints.Num() == 0)
    {
        IssueMoveToCurrentTarget();
        return;
    }

    // raw points
    TArray<FVector> Raw;
    Raw.Reserve(Path->PathPoints.Num());
    for (const FNavPathPoint& P : Path->PathPoints) Raw.Add(P.Location);

    // smooth
    TArray<FVector> Smoothed;
    SmoothCorners(Raw, Smoothed);

    // re-project every smoothed point to be safely inside navmesh
    ActivePathPoints.Reset();
    for (const FVector& P : Smoothed)
    {
        FNavLocation Pj;
        if (NavSys->ProjectPointToNavigation(P, Pj, Extent)) ActivePathPoints.Add(Pj.Location);
        else ActivePathPoints.Add(P); // fallback
    }

    ActivePathIndex = (ActivePathPoints.Num() > 1) ? 1 : 0;
    bFollowingSubPath = true;
    FollowNextSubPoint();
}

void ASimulationRobotPawn::FollowNextSubPoint()
{
    if (!AICon || ActivePathIndex == INDEX_NONE || ActivePathIndex >= ActivePathPoints.Num()) return;

    // ensure each target point sits on navmesh
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FVector Target = ActivePathPoints[ActivePathIndex];
    if (NavSys)
    {
        FNavLocation Pj;
        if (NavSys->ProjectPointToNavigation(Target, Pj, FVector(300, 300, 600)))
            Target = Pj.Location;
    }

    FAIMoveRequest Req;
    Req.SetUsePathfinding(false); // follow our smoothed list directly
    Req.SetAllowPartialPath(false);
    Req.SetAcceptanceRadius(FMath::Max(60.f, AcceptanceRadius * 0.6f));
    Req.SetGoalLocation(Target);

    AICon->MoveTo(Req);
}

// ----------------- cameras -----------------
void ASimulationRobotPawn::SetAerialView(bool bUseAerial)
{
    bUsingAerialView = bUseAerial;

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        if (ThirdPersonCamera) ThirdPersonCamera->SetActive(!bUsingAerialView);
        if (AerialCamera)      AerialCamera->SetActive(bUseAerial);
        if (InteriorCamera)    InteriorCamera->SetActive(false);

        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
        if (bUseAerial && AerialCamera)
            PC->SetViewTarget(this, Params);
        else
        {
            EnsureViewTargetProxy();
            if (ViewTargetProxy) PC->SetViewTarget(ViewTargetProxy, Params);
        }
    }
}

void ASimulationRobotPawn::ForceThirdPersonCamera()
{
    bUsingAerialView = false;
    bUsingInteriorView = false;
    if (ThirdPersonCamera) ThirdPersonCamera->SetActive(true);
    if (AerialCamera)      AerialCamera->SetActive(false);
    if (InteriorCamera)    InteriorCamera->SetActive(false);
    EnsureViewTargetProxy();
}

void ASimulationRobotPawn::EnsureViewTargetProxy()
{
    if (IsValid(ViewTargetProxy)) return;

    if (UWorld* W = GetWorld())
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ViewTargetProxy = W->SpawnActor<ACameraActor>(GetActorLocation(), GetActorRotation(), Params);

        if (ViewTargetProxy && SpringArm)
        {
            ViewTargetProxy->AttachToComponent(SpringArm, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            ViewTargetProxy->SetActorRelativeTransform(
                ThirdPersonCamera ? ThirdPersonCamera->GetRelativeTransform()
                : FTransform(FRotator::ZeroRotator, FVector(0, 0, -SpringArm->TargetArmLength)));
        }
    }
}

// ----------------- checkpoints -----------------
void ASimulationRobotPawn::SetPatrolCheckpoints(const TArray<FVector>& Locs)
{
    PatrolCheckpoints = Locs;
    CurrentWPIndex = 0;
}

// ----------------- screen → world -----------------
bool ASimulationRobotPawn::ScreenToWorldLocation(FVector2D ScreenPos, FVector& WorldLocation)
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        FVector WorldDir;
        if (PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldLocation, WorldDir))
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

// ----------------- threats -----------------
void ASimulationRobotPawn::OnThreatBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (OtherActor && OtherActor != this) NearbyThreats.Add(OtherActor);
}
void ASimulationRobotPawn::OnThreatEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (OtherActor) NearbyThreats.Remove(OtherActor);
}
void ASimulationRobotPawn::DrawThreatDebug()
{
    for (TWeakObjectPtr<AActor> T : NearbyThreats)
    {
        if (!T.IsValid()) continue;
        const FBox B = T->GetComponentsBoundingBox(true);
        DrawDebugBox(GetWorld(), B.GetCenter(), B.GetExtent(), FQuat::Identity, FColor::Red, false, 0.f, 0, 2.f);

        FString Label = TEXT("Threat");
        if (UThreatComponent* TC = T->FindComponentByClass<UThreatComponent>())
            Label = TC->ThreatLabel.ToString();

        DrawDebugString(GetWorld(), B.GetCenter() + FVector(0, 0, B.GetExtent().Z + 50.f), Label, nullptr, FColor::Red, 0.f, true);
    }
}
void ASimulationRobotPawn::UpdateThreatOverlay()
{
    if (!ThreatOverlayWidget) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    TArray<FThreatScreenBox> Boxes;

    for (TWeakObjectPtr<AActor> T : NearbyThreats)
    {
        if (!T.IsValid()) continue;

        const FBox Bounds = T->GetComponentsBoundingBox(true);
        const FVector Min = Bounds.Min;
        const FVector Max = Bounds.Max;

        const FVector C[8] = {
            {Min.X,Min.Y,Min.Z},{Max.X,Min.Y,Min.Z},{Max.X,Max.Y,Min.Z},{Min.X,Max.Y,Min.Z},
            {Min.X,Min.Y,Max.Z},{Max.X,Min.Y,Max.Z},{Max.X,Max.Y,Max.Z},{Min.X,Max.Y,Max.Z}
        };

        FVector2D MinS(FLT_MAX, FLT_MAX), MaxS(-FLT_MAX, -FLT_MAX);
        bool bAny = false;

        for (int i = 0; i < 8; ++i)
        {
            FVector2D S;
            if (PC->ProjectWorldLocationToScreen(C[i], S, true))
            {
                bAny = true;
                MinS.X = FMath::Min(MinS.X, S.X);
                MinS.Y = FMath::Min(MinS.Y, S.Y);
                MaxS.X = FMath::Max(MaxS.X, S.X);
                MaxS.Y = FMath::Max(MaxS.Y, S.Y);
            }
        }
        if (!bAny) continue;

        FThreatScreenBox B; B.Min = MinS; B.Max = MaxS;
        if (UThreatComponent* TC = T->FindComponentByClass<UThreatComponent>())
            B.Label = FText::FromName(TC->ThreatLabel);
        else
            B.Label = FText::FromString(TEXT("Threat"));

        Boxes.Add(B);
    }

    ThreatOverlayWidget->SetBoxes(Boxes);
}
