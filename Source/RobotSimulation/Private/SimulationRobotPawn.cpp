#include "SimulationRobotPawn.h"
#include "GM_Simulation.h"

#include "Kismet/GameplayStatics.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Waypoint.h"

#include "NavigationSystem.h"
#include "NavigationPath.h"

#include "DrawDebugHelpers.h"
#include "Components/InputComponent.h"
#include "Blueprint/UserWidget.h"
#include "ThreatBoxesWidget.h"
#include "ThreatScreenBox.h"
#include "ThreatComponent.h"
#include "Components/PrimitiveComponent.h"

#define LOG_PTR(Name, Ptr) UE_LOG(LogTemp, Log, TEXT("[RobotPawn] %s: %s"), TEXT(Name), Ptr ? *Ptr->GetName() : TEXT("<null>"))

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit)
{
    PrimaryActorTick.bCanEverTick = true;

    // ---------- Third-person camera ----------
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = ThirdPersonArmLength;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bEnableCameraLag = bEnableCamLag;
    SpringArm->CameraLagSpeed = CamLagSpeed;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCam"));
    ThirdPersonCamera->SetupAttachment(SpringArm);
    ThirdPersonCamera->bUsePawnControlRotation = false;

    // ---------- Camera Feeds for UI ----------
    InteriorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("InteriorPivot"));
    InteriorPivot->SetupAttachment(RootComponent);
    InteriorPivot->SetRelativeLocation(FVector(0, 0, 120.f));

    Cam360Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Cam360Capture"));
    Cam360Capture->SetupAttachment(InteriorPivot);
    Cam360Capture->FOVAngle = 90.f;
    Cam360Capture->bCaptureEveryFrame = true;
    Cam360Capture->ProjectionType = ECameraProjectionMode::Perspective;

    RearCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RearCapture"));
    RearCapture->SetupAttachment(RootComponent);
    RearCapture->SetRelativeLocation(FVector(-180.f, 0.f, 120.f));
    RearCapture->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
    RearCapture->FOVAngle = 90.f;
    RearCapture->bCaptureEveryFrame = true;

    SideCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SideCapture"));
    SideCapture->SetupAttachment(RootComponent);
    SideCapture->SetRelativeLocation(FVector(0.f, -140.f, 120.f));
    SideCapture->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    SideCapture->FOVAngle = 90.f;
    SideCapture->bCaptureEveryFrame = true;

    // ---------- Aerial camera ----------
    AerialCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AerialCam"));
    AerialCamera->SetupAttachment(RootComponent);
    AerialCamera->SetRelativeLocation(FVector(0, 0, 5000));
    AerialCamera->SetRelativeRotation(FRotator(-90, 0, 0));
    AerialCamera->SetAutoActivate(false);

    // ---------- Lights ----------
    HeadlightLeft = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightLeft"));
    HeadlightRight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightRight"));
    HeadlightLeft->SetupAttachment(RootComponent);
    HeadlightRight->SetupAttachment(RootComponent);
    HeadlightLeft->SetRelativeLocation(FVector(120.f, -40.f, 40.f));
    HeadlightRight->SetRelativeLocation(FVector(120.f, 40.f, 40.f));
    HeadlightLeft->Intensity = 5000.f;
    HeadlightRight->Intensity = 5000.f;
    HeadlightLeft->SetVisibility(false);
    HeadlightRight->SetVisibility(false);

    TailLightLeft = CreateDefaultSubobject<USpotLightComponent>(TEXT("TailLightLeft"));
    TailLightRight = CreateDefaultSubobject<USpotLightComponent>(TEXT("TailLightRight"));
    TailLightLeft->SetupAttachment(RootComponent);
    TailLightRight->SetupAttachment(RootComponent);
    TailLightLeft->SetRelativeLocation(FVector(-120.f, -40.f, 40.f));
    TailLightRight->SetRelativeLocation(FVector(-120.f, 40.f, 40.f));
    TailLightLeft->SetLightColor(FLinearColor(1.f, 0.05f, 0.05f));
    TailLightRight->SetLightColor(FLinearColor(1.f, 0.05f, 0.05f));
    TailLightLeft->Intensity = 0.f;
    TailLightRight->Intensity = 0.f;
    TailLightLeft->SetVisibility(false);
    TailLightRight->SetVisibility(false);

    // ---------- Threat sensor ----------
    ThreatSensor = CreateDefaultSubobject<USphereComponent>(TEXT("ThreatSensor"));
    ThreatSensor->SetupAttachment(RootComponent);
    ThreatSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ThreatSensor->SetCollisionObjectType(ECC_WorldDynamic);
    ThreatSensor->SetCollisionResponseToAllChannels(ECR_Overlap);
    ThreatSensor->SetSphereRadius(ThreatSenseRadius);

    AutoPossessPlayer = EAutoReceiveInput::Player0;
    AutoPossessAI = EAutoPossessAI::Disabled;
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
    if (!HeadlightRight) HeadlightRight = HeadlightLeft;

    if (!TailLightLeft)  TailLightLeft = FindComponentByClass<USpotLightComponent>();
    if (!TailLightRight) TailLightRight = TailLightLeft;

    if (!ThreatSensor)   ThreatSensor = FindComponentByClass<USphereComponent>();
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    ResolveCriticalComponents();

    // ---------- Create RTs for the three feeds ----------
    Cam360RT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_Cam360"));
    Cam360RT->InitAutoFormat(512, 288);
    Cam360RT->ClearColor = FLinearColor::Black;
    Cam360RT->TargetGamma = 2.2f;

    RearRT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_Rear"));
    RearRT->InitAutoFormat(512, 288);
    RearRT->ClearColor = FLinearColor::Black;
    RearRT->TargetGamma = 2.2f;

    SideRT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_Side"));
    SideRT->InitAutoFormat(512, 288);
    SideRT->ClearColor = FLinearColor::Black;
    SideRT->TargetGamma = 2.2f;

    if (Cam360Capture) Cam360Capture->TextureTarget = Cam360RT;
    if (RearCapture)   RearCapture->TextureTarget = RearRT;
    if (SideCapture)   SideCapture->TextureTarget = SideRT;

    if (AGM_Simulation* GM = GetWorld()->GetAuthGameMode<AGM_Simulation>())
        GM->NotifyRobotReady(this);

    ForceThirdPersonCamera();
    EnsureViewTargetProxy();

    // Always show the mouse + allow UI clicks
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        ApplyAlwaysInteractiveInput(PC);

        // HUD (non-interactive overlay) — must NOT block clicks
        if (HUDWidgetClass)
        {
            HUDWidget = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
            if (HUDWidget)
            {
                HUDWidget->AddToViewport(/*Z*/ 1);
                HUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            }
        }

        // Threat boxes overlay — also must NOT block
        if (ThreatOverlayWidgetClass)
        {
            ThreatOverlayWidget = CreateWidget<UThreatBoxesWidget>(PC, ThreatOverlayWidgetClass);
            if (ThreatOverlayWidget)
            {
                ThreatOverlayWidget->AddToViewport(/*Z*/ 10);
                ThreatOverlayWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            }
        }
    }

    // Waypoints (optional fallback)
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

    // Start with headlights off (manual control) and brake lights off
    SetHeadlightsOn(false);
    UpdateBrakeLightState(false);
}

void ASimulationRobotPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AGM_Simulation* GM = GetWorld()->GetAuthGameMode<AGM_Simulation>())
        if (GM->GetRobotPawn() == this) GM->NotifyRobotReady(nullptr);

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        RemoveAuxInput(PC);

    Super::EndPlay(EndPlayReason);
}

static float KmhFromCms(float CmPerSec) { return CmPerSec * 0.036f; }
static float CmpsFromKmh(float Kmh) { return Kmh / 0.036f; }

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Speed
    if (auto* M = GetVehicleMovementComponent())
        SpeedKmh = M->GetForwardSpeed() * 0.036f;
    else
        SpeedKmh = 0.f;

    // Strict clamp: NEVER leave navmesh
    if (bHardClampToNavmesh)
    {
        if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            FNavLocation Projected;
            const FVector Extent(300.f, 300.f, 600.f);
            if (NavSys->ProjectPointToNavigation(GetActorLocation(), Projected, Extent))
            {
                const float Dist2D = FVector::Dist2D(Projected.Location, GetActorLocation());

                if (Dist2D > StrictClampDistance)
                {
                    SetActorLocation(Projected.Location, /*bSweep=*/true);
                }
                else if (Dist2D > MaxOffNavDistance)
                {
                    const FVector NewLoc = FMath::VInterpTo(GetActorLocation(), Projected.Location, DeltaTime, 24.f);
                    SetActorLocation(NewLoc, /*bSweep=*/true);
                }
            }
        }
    }

    // 360 auto-pan (left <-> right)
    Cam360Time += DeltaTime;
    if (InteriorPivot)
    {
        const float RadPerSec = FMath::DegreesToRadians(Cam360YawSpeed);
        const float Yaw = FMath::Sin(Cam360Time * RadPerSec) * Cam360YawAmplitude;
        FRotator R = InteriorPivot->GetRelativeRotation();
        R.Yaw = Yaw;
        InteriorPivot->SetRelativeRotation(R);
    }

    if (bDrawThreatBoxes) DrawThreatDebug();
    UpdateThreatOverlay();

    if (bIsPatrolMode)
        DriveAlongPath(DeltaTime);
}

// ----------------- Lights helpers -----------------
void ASimulationRobotPawn::ApplyHeadlightVisibility()
{
    if (HeadlightLeft)  HeadlightLeft->SetVisibility(bHeadlightsOn);
    if (HeadlightRight) HeadlightRight->SetVisibility(bHeadlightsOn);
}

void ASimulationRobotPawn::SetHeadlightsOn(bool bOn)
{
    bHeadlightsOn = bOn;
    ApplyHeadlightVisibility();
}

void ASimulationRobotPawn::UpdateBrakeLightState(bool bBraking)
{
    bBrakingNow = bBraking;

    const float BrakeIntensity = bBraking ? 12000.f : 0.f;

    if (TailLightLeft)
    {
        TailLightLeft->SetIntensity(BrakeIntensity);
        TailLightLeft->SetVisibility(bBraking);
    }
    if (TailLightRight)
    {
        TailLightRight->SetIntensity(BrakeIntensity);
        TailLightRight->SetVisibility(bBraking);
    }
}

// --------------------------------------------------

void ASimulationRobotPawn::SetupPlayerInputComponent(UInputComponent* P)
{
    Super::SetupPlayerInputComponent(P);
    BindCommonInputs(P);
}

void ASimulationRobotPawn::BindCommonInputs(UInputComponent* IC)
{
    if (!IC) return;
    IC->BindAxis("MoveForward", this, &ASimulationRobotPawn::ThrottleInput);
    IC->BindAxis("MoveRight", this, &ASimulationRobotPawn::SteeringInput);
    IC->BindAxis("Handbrake", this, &ASimulationRobotPawn::HandbrakeInput);
    IC->BindAxis("LookUp", this, &ASimulationRobotPawn::LookUp);
    IC->BindAxis("Turn", this, &ASimulationRobotPawn::Turn);
    IC->BindAction("RotateCamera", IE_Pressed, this, &ASimulationRobotPawn::StartCameraRotate);
    IC->BindAction("RotateCamera", IE_Released, this, &ASimulationRobotPawn::StopCameraRotate);
}

void ASimulationRobotPawn::InstallAuxInput(APlayerController* PC)
{
    if (!PC || AuxInput) return;
    AuxInput = NewObject<UInputComponent>(this, TEXT("AuxInputComponent"));
    AuxInput->bBlockInput = false;
    AuxInput->Priority = 10;
    AuxInput->RegisterComponent();
    BindCommonInputs(AuxInput);
    PC->PushInputComponent(AuxInput);
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
    Mode.SetWidgetToFocus(nullptr);
    PC->SetInputMode(Mode);
    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    PC->bEnableMouseOverEvents = true;
}

// Input helpers (manual control when patrol is OFF)
void ASimulationRobotPawn::ThrottleInput(float Val)
{
    if (bIsPatrolMode) return;

    auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
    if (!M) return;

    const float fwdSpeed = SpeedKmh;
    const bool bWantReverse = (Val < -KINDA_SMALL_NUMBER);
    const float amt = FMath::Abs(Val);

    // Forward speed limit (only for forward)
    if (bSpeedLimited && !bWantReverse && fwdSpeed >= MaxSpeedKmh)
    {
        M->SetThrottleInput(0.f);
        return;
    }

    if (bWantReverse)
    {
        // If rolling forward, brake first
        if (fwdSpeed > 1.0f && M->GetCurrentGear() >= 0)
        {
            M->SetBrakeInput(amt);
            M->SetThrottleInput(0.f);
        }
        else
        {
            if (M->GetCurrentGear() >= 0)
                M->SetTargetGear(-1, true);
            M->SetBrakeInput(0.f);
            M->SetThrottleInput(amt); // throttle is positive; gear decides direction
        }
    }
    else
    {
        // Forward
        if (M->GetCurrentGear() < 0 && fwdSpeed < 1.0f)
            M->SetTargetGear(1, true);

        M->SetBrakeInput(0.f);
        M->SetThrottleInput(Val);
    }
}
void ASimulationRobotPawn::SteeringInput(float Val)
{
    if (bIsPatrolMode) return;
    if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        M->SetSteeringInput(Val);
}
void ASimulationRobotPawn::HandbrakeInput(float Val)
{
    if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        M->SetHandbrakeInput(Val > KINDA_SMALL_NUMBER);

    // Braking state for brake lights (manual handbrake)
    bHandbrakeActiveManual = (Val > KINDA_SMALL_NUMBER);
    UpdateBrakeLightState(bHandbrakeActiveManual || bBrakingNow);
}
void ASimulationRobotPawn::StartCameraRotate() { bRotatingCamera = true; }
void ASimulationRobotPawn::StopCameraRotate() { bRotatingCamera = false; }
void ASimulationRobotPawn::LookUp(float V)
{
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(V * LookUpSpeed * GetWorld()->DeltaTimeSeconds, 0, 0));
}
void ASimulationRobotPawn::Turn(float V)
{
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(0, V * TurnSpeed * GetWorld()->DeltaTimeSeconds, 0));
}

// Modes
void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    if (auto* C = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        C->EngineSetup.MaxRPM = 3000.f; // simple cap
}
void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (bIsPatrolMode)
    {
        SetCheckpointMeshesHidden(true); // hide visual markers
        CurrentWPIndex = 0;

        FVector Goal = GetActorLocation();
        if (!PatrolCheckpoints.IsEmpty())             Goal = PatrolCheckpoints[0];
        else if (Waypoints.Num() > 0 && Waypoints[0]) Goal = Waypoints[0]->GetActorLocation();

        BuildPathTo(Goal);

        if (PC) { InstallAuxInput(PC); ApplyAlwaysInteractiveInput(PC); }
    }
    else
    {
        SetCheckpointMeshesHidden(false);

        ActivePathPoints.Reset();
        ActivePathCumLen.Reset();
        ActivePathTotalLen = 0.f;
        CachedClosestSeg = 0;

        if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        {
            M->SetSteeringInput(0.f);
            M->SetThrottleInput(0.f);
            M->SetBrakeInput(0.f);
            M->SetHandbrakeInput(true);
        }

        if (PC)
        {
            RemoveAuxInput(PC);
            PC->Possess(this);
            ApplyAlwaysInteractiveInput(PC);
        }
    }
}
void ASimulationRobotPawn::BeginMission() { if (!bIsPatrolMode) TogglePatrolMode(); }
void ASimulationRobotPawn::EndMission()
{
    if (bIsPatrolMode) TogglePatrolMode();
    SetAerialView(true);
}
void ASimulationRobotPawn::EndSimulation()
{
    if (bIsPatrolMode) TogglePatrolMode();

    PatrolCheckpoints.Reset();
    ActivePathPoints.Reset();
    ActivePathCumLen.Reset();
    ActivePathTotalLen = 0.f;
    CachedClosestSeg = 0;
    CurrentWPIndex = 0;

    SetAerialView(true);
}

void ASimulationRobotPawn::ToggleLights()
{
    // FRONT white LEDs ONLY (manual)
    SetHeadlightsOn(!bHeadlightsOn);
}
void ASimulationRobotPawn::ChangeView()
{
    bUsingAerialView = !bUsingAerialView;
    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
        if (bUsingAerialView && AerialCamera)
        {
            AerialCamera->SetActive(true);
            ThirdPersonCamera->SetActive(false);
            PC->SetViewTarget(this, Params);
        }
        else
        {
            ThirdPersonCamera->SetActive(true);
            AerialCamera->SetActive(false);
            EnsureViewTargetProxy();
            if (ViewTargetProxy) PC->SetViewTarget(ViewTargetProxy, Params);
        }
    }
}
void ASimulationRobotPawn::SetAerialView(bool bUseAerial)
{
    bUsingAerialView = bUseAerial;
    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        if (ThirdPersonCamera) ThirdPersonCamera->SetActive(!bUsingAerialView);
        if (AerialCamera)      AerialCamera->SetActive(bUseAerial);
        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
        if (bUseAerial && AerialCamera) PC->SetViewTarget(this, Params);
        else { EnsureViewTargetProxy(); if (ViewTargetProxy) PC->SetViewTarget(ViewTargetProxy, Params); }
    }
}
void ASimulationRobotPawn::ForceThirdPersonCamera()
{
    bUsingAerialView = false;
    if (ThirdPersonCamera) ThirdPersonCamera->SetActive(true);
    if (AerialCamera)      AerialCamera->SetActive(false);
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

// -------- Path building --------
static void AppendIfFar(TArray<FVector>& Points, const FVector& P, float MinDist)
{
    if (Points.Num() == 0 || FVector::DistSquared2D(Points.Last(), P) > FMath::Square(MinDist))
    {
        Points.Add(P);
    }
}

bool ASimulationRobotPawn::ProjectToNav(const FVector& In, FVector& Out) const
{
    if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        FNavLocation Pj;
        const FVector Extent(300.f, 300.f, 600.f);
        if (NavSys->ProjectPointToNavigation(In, Pj, Extent))
        {
            Out = Pj.Location; return true;
        }
    }
    Out = In; return false;
}

void ASimulationRobotPawn::SmoothPathInPlace(TArray<FVector>& Pts, float Alpha)
{
    if (Pts.Num() < 3 || Alpha <= 0.f) return;
    TArray<FVector> Copy = Pts;
    for (int32 i = 1; i < Pts.Num() - 1; ++i)
    {
        Pts[i] = FMath::Lerp(Copy[i], (Copy[i - 1] + Copy[i + 1]) * 0.5f, FMath::Clamp(Alpha, 0.f, 0.95f));
    }
}

bool ASimulationRobotPawn::BuildPathTo(const FVector& Goal)
{
    ActivePathPoints.Reset();
    ActivePathCumLen.Reset();
    ActivePathTotalLen = 0.f;
    CachedClosestSeg = 0;

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys) return false;

    // project start/goal
    FVector StartLoc = GetActorLocation(), StartP = StartLoc, GoalP = Goal;
    ProjectToNav(StartLoc, StartP);
    ProjectToNav(Goal, GoalP);

    UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), StartP, GoalP, this);
    if (!Path || Path->PathPoints.Num() < 2) return false;

    // Densify + project each lerped point to navmesh (centerline-ish)
    TArray<FVector> Dense;
    AppendIfFar(Dense, Path->PathPoints[0], MinPointSpacing);

    for (int32 i = 0; i < Path->PathPoints.Num() - 1; ++i)
    {
        const FVector A = Path->PathPoints[i];
        const FVector B = Path->PathPoints[i + 1];

        const float SegLen = FVector::Dist(A, B);
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(SegLen / FMath::Max(10.f, CorridorStep)));

        for (int32 s = 1; s <= Steps; ++s)
        {
            const float T = (float)s / (float)Steps;
            const FVector Raw = FMath::Lerp(A, B, T);
            FVector P;
            ProjectToNav(Raw, P);
            AppendIfFar(Dense, P, MinPointSpacing);
        }
    }

    SmoothPathInPlace(Dense, SmoothKernel);
    ActivePathPoints = MoveTemp(Dense);
    RecomputeCumulativeLength();

    return ActivePathPoints.Num() >= 2;
}

void ASimulationRobotPawn::RecomputeCumulativeLength()
{
    ActivePathCumLen.SetNum(ActivePathPoints.Num());
    float Acc = 0.f;
    ActivePathCumLen[0] = 0.f;
    for (int32 i = 1; i < ActivePathPoints.Num(); ++i)
    {
        Acc += FVector::Dist(ActivePathPoints[i - 1], ActivePathPoints[i]);
        ActivePathCumLen[i] = Acc;
    }
    ActivePathTotalLen = Acc;
    CachedClosestSeg = 0;
}

bool ASimulationRobotPawn::FindClosestOnPath2D(const FVector& Pos, int32& OutSeg, float& OutT, FVector& OutPoint) const
{
    if (ActivePathPoints.Num() < 2) return false;

    const int32 MaxSearchAhead = 32;
    const int32 StartSeg = FMath::Clamp(CachedClosestSeg - 1, 0, ActivePathPoints.Num() - 2);
    const int32 EndSeg = FMath::Min(StartSeg + MaxSearchAhead, ActivePathPoints.Num() - 2);

    float BestDist2 = TNumericLimits<float>::Max();
    int32 BestSeg = StartSeg; float BestT = 0.f; FVector BestP = ActivePathPoints[StartSeg];

    for (int32 i = StartSeg; i <= EndSeg; ++i)
    {
        const FVector A = ActivePathPoints[i];
        const FVector B = ActivePathPoints[i + 1];
        const FVector AB = B - A;
        const float AB2 = FMath::Max(AB.SizeSquared2D(), 1.f);
        const float T = FMath::Clamp(FVector::DotProduct((Pos - A), AB) / AB2, 0.f, 1.f);
        const FVector P = A + AB * T;
        const float D2 = FVector::DistSquared2D(Pos, P);
        if (D2 < BestDist2)
        {
            BestDist2 = D2; BestSeg = i; BestT = T; BestP = P;
        }
    }

    OutSeg = BestSeg; OutT = BestT; OutPoint = BestP;
    return true;
}

FVector ASimulationRobotPawn::SamplePathAtS(float S, int32& IO_Seg) const
{
    S = FMath::Clamp(S, 0.f, ActivePathTotalLen);

    // fast-path
    int32 i = IO_Seg;
    if (i < 0) i = 0;
    if (i >= ActivePathCumLen.Num() - 1) i = ActivePathCumLen.Num() - 2;

    while (i < ActivePathCumLen.Num() - 1 && ActivePathCumLen[i + 1] < S) ++i;
    while (i > 0 && ActivePathCumLen[i] > S) --i;

    IO_Seg = i;

    const float S0 = ActivePathCumLen[i];
    const float S1 = ActivePathCumLen[i + 1];
    const float T = (S1 > S0) ? (S - S0) / (S1 - S0) : 0.f;

    return FMath::Lerp(ActivePathPoints[i], ActivePathPoints[i + 1], T);
}

// ---------- Corridor helpers ----------
void ASimulationRobotPawn::MeasureCorridor(const FVector& Base, const FVector& Right2D, float MaxHalfWidth, float Step,
    float& OutLeft, float& OutRight) const
{
    auto MeasureDir = [&](const FVector& Dir) -> float
        {
            float LastGood = 0.f;
            for (float d = Step; d <= MaxHalfWidth; d += Step)
            {
                const FVector Test = Base + Dir * d;
                FVector OnNav;
                ProjectToNav(Test, OnNav);
                const float Dist = FVector::Dist2D(Test, OnNav);
                if (Dist > Step * 0.45f) break; // boundary reached
                LastGood = d;
            }
            return LastGood;
        };

    OutRight = MeasureDir(Right2D);
    OutLeft = MeasureDir(-Right2D);
}

FVector ASimulationRobotPawn::CenterPointInCorridor(const FVector& Base, const FVector& Fwd2D, float MaxHalfWidth, float Step) const
{
    FVector F = Fwd2D; F.Z = 0.f;
    if (!F.Normalize())
    {
        F = GetActorForwardVector(); F.Z = 0.f; F.Normalize();
    }
    const FVector Right2D(-F.Y, F.X, 0.f);

    float WL = 0.f, WR = 0.f;
    MeasureCorridor(Base, Right2D, MaxHalfWidth, Step, WL, WR);

    const float Offset = (WR - WL) * 0.5f;
    FVector Center = Base + Right2D * Offset;

    FVector OnNav = Center;
    ProjectToNav(Center, OnNav);
    return OnNav;
}

// -------- Driving (AI) --------
void ASimulationRobotPawn::DriveAlongPath(float Dt)
{
    if (ActivePathPoints.Num() < 2) return;

    int32  ClosestSeg = CachedClosestSeg;
    float  ClosestT = 0.f;
    FVector ClosestP = ActivePathPoints[0];
    FindClosestOnPath2D(GetActorLocation(), ClosestSeg, ClosestT, ClosestP);
    CachedClosestSeg = ClosestSeg;

    const float SNow = ActivePathCumLen[ClosestSeg] +
        ClosestT * (ActivePathCumLen[ClosestSeg + 1] - ActivePathCumLen[ClosestSeg]);

    float Ld = FMath::Clamp(LookaheadMinCM + LookaheadGainPerKmh * SpeedKmh, LookaheadMinCM, LookaheadMaxCM);

    int32 TmpSeg = ClosestSeg;
    const FVector AheadA = SamplePathAtS(SNow + 80.f, TmpSeg);
    const FVector AheadB = SamplePathAtS(SNow + 160.f, TmpSeg);
    FVector PathTangent = (AheadB - AheadA); PathTangent.Z = 0.f; PathTangent.Normalize();

    auto MakeTarget = [&](float LdLocal) -> FVector
        {
            int32 SegForSample = ClosestSeg;
            const FVector Raw = SamplePathAtS(SNow + LdLocal, SegForSample);
            const FVector Mid = CenterPointInCorridor(
                Raw,
                PathTangent.IsNearlyZero() ? GetActorForwardVector() : PathTangent,
                CorridorProbeHalfWidth, CorridorProbeStep);
            FVector Blended = FMath::Lerp(Raw, Mid, FMath::Clamp(CenteringAlpha, 0.f, 1.f));
            FVector OnNav = Blended; ProjectToNav(Blended, OnNav);
            return OnNav;
        };

    FVector TargetOnNav = MakeTarget(Ld);

    const FVector Loc = GetActorLocation();
    const FVector Fwd = GetActorForwardVector();
    const FVector Rt = GetActorRightVector();

    auto ComputeSteer = [&](const FVector& Tgt) -> float
        {
            const FVector ToT = Tgt - Loc;
            const float x = FVector::DotProduct(Fwd, ToT);
            const float y = FVector::DotProduct(Rt, ToT);
            const float L = FMath::Max(1.f, WheelbaseCM);
            const float Ld2 = FMath::Max(1.f, x * x + y * y);
            const float SteerRad = FMath::Atan2(2.f * L * y, Ld2);
            const float MaxSteerRad = FMath::DegreesToRadians(MaxSteerAngleDeg);
            return FMath::Clamp(SteerRad / MaxSteerRad, -1.f, 1.f);
        };

    float SteerInput = ComputeSteer(TargetOnNav);

    if (FMath::Abs(SteerInput) > SteerTightThreshold)
    {
        Ld = FMath::Max(LookaheadMinCM, Ld * FMath::Clamp(LookaheadTightScale, 0.2f, 1.f));
        TargetOnNav = MakeTarget(Ld);
        SteerInput = ComputeSteer(TargetOnNav);
    }

    const FVector ToT = TargetOnNav - Loc;
    const float x = FVector::DotProduct(Fwd, ToT);
    const float y = FVector::DotProduct(Rt, ToT);
    const float L = FMath::Max(1.f, WheelbaseCM);
    const float Ld2 = FMath::Max(1.f, x * x + y * y);
    const float SteerRad = FMath::Atan2(2.f * L * y, Ld2);

    const float MaxSteerRad = FMath::DegreesToRadians(MaxSteerAngleDeg);
    const float kappa_pp_cm = (2.f * y) / FMath::Max(1.f, Ld2);
    const float kappa_pp_m = kappa_pp_cm * 100.f;
    float Vmax_mps = (FMath::Abs(kappa_pp_m) > 1e-5f)
        ? FMath::Sqrt(FMath::Max(0.0f, LateralAccelMax / FMath::Abs(kappa_pp_m)))
        : CmpsFromKmh(MaxSpeedKmh) / 100.f;

    const float L_m = WheelbaseCM * 0.01f;
    const float Rmin = FMath::Max(0.1f, L_m / FMath::Max(1e-3f, FMath::Tan(MaxSteerRad)));
    const float VcapSteer_mps = FMath::Sqrt(FMath::Max(0.0f, LateralAccelMax * Rmin));

    float DesiredKmh = FMath::Min(MaxSpeedKmh, FMath::Min(Vmax_mps, VcapSteer_mps) * 3.6f);
    DesiredKmh = FMath::Max(DesiredKmh, MinCurveSpeedKmh);

    const float Remaining = ActivePathTotalLen - SNow;
    if (Remaining < 800.f)
    {
        const float t = FMath::Clamp(Remaining / 800.f, 0.f, 1.f);
        DesiredKmh = FMath::Lerp(6.f, DesiredKmh, t);
    }

    if (FilteredDesiredSpeedKmh <= KINDA_SMALL_NUMBER)
        FilteredDesiredSpeedKmh = DesiredKmh;
    else
        FilteredDesiredSpeedKmh = FMath::FInterpTo(FilteredDesiredSpeedKmh, DesiredKmh, Dt, 2.0f);

    const float CurrSpeedKmh = SpeedKmh;
    const float Err = FilteredDesiredSpeedKmh - CurrSpeedKmh;

    float RawThrottle = 0.f;
    float RawBrake = 0.f;

    if (Err > CoastBandKmh)
    {
        RawThrottle = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 12.f), FVector2D(0.10f, 1.f), Err);
        RawBrake = 0.f;
    }
    else if (Err < -BrakeBandKmh)
    {
        RawThrottle = 0.f;
        RawBrake = FMath::GetMappedRangeValueClamped(FVector2D(-30.f, 0.f), FVector2D(1.f, 0.f), Err);
    }
    else
    {
        RawThrottle = 0.f; RawBrake = 0.f; // coast band
    }

    SmoothedSteer = FMath::FInterpTo(SmoothedSteer, SteerInput, Dt, SteeringSmoothing);
    SmoothedThrottle = FMath::FInterpTo(SmoothedThrottle, RawThrottle, Dt, ThrottleSmoothing);
    SmoothedBrake = FMath::FInterpTo(SmoothedBrake, RawBrake, Dt, BrakeSmoothing);

    const bool bAI_Braking = (SmoothedBrake > 0.05f);
    UpdateBrakeLightState(bAI_Braking || bHandbrakeActiveManual);

    if (auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
    {
        M->SetSteeringInput(SmoothedSteer);
        M->SetBrakeInput(SmoothedBrake);
        M->SetThrottleInput(SmoothedThrottle);

        const float AngleDeg = FMath::RadiansToDegrees(FMath::Abs(SteerRad));
        const bool bVerySharp = AngleDeg > 0.9f * MaxSteerAngleDeg && CurrSpeedKmh > 8.f;
        M->SetHandbrakeInput(bVerySharp && Err < -BrakeBandKmh);
    }

    if (Remaining <= GoalAcceptanceRadius)
    {
        if (!PatrolCheckpoints.IsEmpty())
        {
            CurrentWPIndex = (CurrentWPIndex + 1) % PatrolCheckpoints.Num();
            BuildPathTo(PatrolCheckpoints[CurrentWPIndex]);
        }
        else if (Waypoints.Num() > 0)
        {
            CurrentWPIndex = (CurrentWPIndex + 1) % Waypoints.Num();
            if (Waypoints[CurrentWPIndex])
                BuildPathTo(Waypoints[CurrentWPIndex]->GetActorLocation());
        }
    }
}

// Screen → world
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
                WorldLocation = Hit.Location; return true;
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

// Threats
void ASimulationRobotPawn::OnThreatBegin(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    if (OtherActor && OtherActor != this) NearbyThreats.Add(OtherActor);
}
void ASimulationRobotPawn::OnThreatEnd(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
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
        {
            Label = TC->ThreatLabel.ToString();
        }

        DrawDebugString(
            GetWorld(),
            B.GetCenter() + FVector(0, 0, B.GetExtent().Z + 50.f),
            Label,
            nullptr,
            FColor::Red,
            0.f,
            true);
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
        const FVector Min = Bounds.Min, Max = Bounds.Max;
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
                MinS.X = FMath::Min(MinS.X, S.X); MinS.Y = FMath::Min(MinS.Y, S.Y);
                MaxS.X = FMath::Max(MaxS.X, S.X); MaxS.Y = FMath::Max(MaxS.Y, S.Y);
            }
        }
        if (!bAny) continue;
        FThreatScreenBox Bx; Bx.Min = MinS; Bx.Max = MaxS;
        Boxes.Add(Bx);
    }
    ThreatOverlayWidget->SetBoxes(Boxes);
}

// -------- Blueprint helpers / API --------
int32 ASimulationRobotPawn::GetThreatCount() const
{
    return NearbyThreats.Num();
}

void ASimulationRobotPawn::SetPatrolCheckpoints(const TArray<FVector>& CheckpointLocations)
{
    PatrolCheckpoints = CheckpointLocations;
    CurrentWPIndex = 0;

    if (bIsPatrolMode)
    {
        if (!PatrolCheckpoints.IsEmpty())
            BuildPathTo(PatrolCheckpoints[0]);
        else if (Waypoints.Num() > 0 && Waypoints[0])
            BuildPathTo(Waypoints[0]->GetActorLocation());
    }
}

// Hide/Show all checkpoint visuals (Blueprint class or Tag)
void ASimulationRobotPawn::SetCheckpointMeshesHidden(bool bHide)
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<AActor*> Targets;

    if (CheckpointMarkerClass)
    {
        UGameplayStatics::GetAllActorsOfClass(World, CheckpointMarkerClass, Targets);
    }
    else if (!CheckpointMarkerTag.IsNone())
    {
        TArray<AActor*> All;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), All);
        for (AActor* A : All)
        {
            if (A && A->Tags.Contains(CheckpointMarkerTag))
            {
                Targets.Add(A);
            }
        }
    }

    for (AActor* A : Targets)
    {
        if (!A) continue;

        TArray<UPrimitiveComponent*> PrimComps;
        A->GetComponents<UPrimitiveComponent>(PrimComps);
        for (UPrimitiveComponent* C : PrimComps)
        {
            if (!C) continue;
            C->SetVisibility(!bHide, true);
            C->SetHiddenInGame(bHide, true);
        }
    }
}
