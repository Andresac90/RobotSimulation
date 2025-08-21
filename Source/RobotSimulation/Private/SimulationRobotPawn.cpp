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

#include "DrawDebugHelpers.h"
#include "ThreatComponent.h"

// overlay
#include "ThreatBoxesWidget.h"
#include "ThreatScreenBox.h"

ASimulationRobotPawn::ASimulationRobotPawn(const FObjectInitializer& ObjInit)
    : Super(ObjInit.SetDefaultSubobjectClass<UPatrolVehicleMovementComponent>(AWheeledVehiclePawn::VehicleMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 500.f;
    SpringArm->bUsePawnControlRotation = false;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCam"));
    ThirdPersonCamera->SetupAttachment(SpringArm);
    ThirdPersonCamera->bUsePawnControlRotation = false;

    AerialCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AerialCam"));
    AerialCamera->SetupAttachment(RootComponent);
    AerialCamera->SetRelativeLocation(FVector(0, 0, 5000));
    AerialCamera->SetRelativeRotation(FRotator(-90, 0, 0));
    AerialCamera->SetAutoActivate(false);

    Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
    Headlight->SetupAttachment(RootComponent);
    Headlight->Intensity = 5000.f;

    ThreatSensor = CreateDefaultSubobject<USphereComponent>(TEXT("ThreatSensor"));
    ThreatSensor->SetupAttachment(RootComponent);
    ThreatSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ThreatSensor->SetCollisionObjectType(ECC_WorldDynamic);
    ThreatSensor->SetCollisionResponseToAllChannels(ECR_Overlap);
    ThreatSensor->SetSphereRadius(ThreatSenseRadius);

    // player owns in planning; AI only when toggled
    AutoPossessPlayer = EAutoReceiveInput::Player0;
    AutoPossessAI = EAutoPossessAI::Disabled;
    AIControllerClass = ARobotAIController::StaticClass();
}

void ASimulationRobotPawn::BeginPlay()
{
    Super::BeginPlay();

    if (AGM_Simulation* GM = GetWorld()->GetAuthGameMode<AGM_Simulation>())
        GM->NotifyRobotReady(this);

    ForceThirdPersonCamera();
    EnsureViewTargetProxy();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;

        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);

        // overlay widget
        if (ThreatOverlayWidgetClass)
        {
            ThreatOverlayWidget = CreateWidget<UThreatBoxesWidget>(PC, ThreatOverlayWidgetClass);
            if (ThreatOverlayWidget) ThreatOverlayWidget->AddToViewport(50);
        }
    }

    // designer-authored waypoints
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), Waypoints);
    Waypoints.Sort([](const AActor& A, const AActor& B)
        {
            const AWaypoint* WA = Cast<AWaypoint>(&A);
            const AWaypoint* WB = Cast<AWaypoint>(&B);
            return (WA && WB) ? WA->PatrolOrder < WB->PatrolOrder : false;
        });

    if (!bSpeedLimited) ToggleSpeedLimit();

    ThreatSensor->OnComponentBeginOverlap.AddDynamic(this, &ASimulationRobotPawn::OnThreatBegin);
    ThreatSensor->OnComponentEndOverlap.AddDynamic(this, &ASimulationRobotPawn::OnThreatEnd);
}

void ASimulationRobotPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AGM_Simulation* GM = GetWorld()->GetAuthGameMode<AGM_Simulation>())
        if (GM->GetRobotPawn() == this) GM->NotifyRobotReady(nullptr);

    Super::EndPlay(EndPlayReason);
}

void ASimulationRobotPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    SpeedKmh = 0.f;
    if (auto* M = GetVehicleMovementComponent())
        SpeedKmh = M->GetForwardSpeed() * 0.036f;

    if (bDrawThreatBoxes) DrawThreatDebug(); // 3D debug
    UpdateThreatOverlay();                   // 2D rectangles

    const int32 Total = (PatrolCheckpoints.Num() > 0) ? PatrolCheckpoints.Num() : Waypoints.Num();
    OnUpdateHUD(SpeedKmh, bSpeedLimited, bLightsOn, bIsPatrolMode,
        NearbyThreats.Num(), Total > 0 ? CurrentWPIndex + 1 : 0, Total);
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

// --- input helpers ---
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
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(V * LookUpSpeed * GetWorld()->DeltaTimeSeconds, 0, 0));
}
void ASimulationRobotPawn::Turn(float V)
{
    if (bRotatingCamera && !bUsingAerialView && SpringArm && FMath::Abs(V) > KINDA_SMALL_NUMBER)
        SpringArm->AddLocalRotation(FRotator(0, V * TurnSpeed * GetWorld()->DeltaTimeSeconds, 0));
}

// --- modes ---
void ASimulationRobotPawn::ToggleSpeedLimit()
{
    bSpeedLimited = !bSpeedLimited;
    ApplySpeedLimit();
}
void ASimulationRobotPawn::ApplySpeedLimit()
{
    if (auto* C = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
        C->EngineSetup.MaxRPM = 3000.f; // input clamps throttle
}

void ASimulationRobotPawn::TogglePatrolMode()
{
    bIsPatrolMode = !bIsPatrolMode;

    if (bIsPatrolMode)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->UnPossess();

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
        IssueMoveToCurrentTarget();

        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            EnableInput(PC); // keep hotkeys alive
            PC->bShowMouseCursor = true; PC->bEnableClickEvents = true; PC->bEnableMouseOverEvents = true;
            FInputModeGameAndUI Mode; Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); Mode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(Mode);
        }
    }
    else
    {
        if (AICon)
        {
            if (auto* PF = AICon->GetPathFollowingComponent()) PF->OnRequestFinished.RemoveAll(this);
            AICon->StopMovement(); AICon->UnPossess();
        }

        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            PC->Possess(this);
            PC->bShowMouseCursor = true; PC->bEnableClickEvents = true; PC->bEnableMouseOverEvents = true;
            FInputModeGameAndUI Mode; Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); Mode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(Mode);
        }
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
        ForceThirdPersonCamera();
        FViewTargetTransitionParams Params; Params.BlendTime = CameraBlendTime; Params.BlendFunction = VTBlend_Cubic;
        if (bUsingAerialView && AerialCamera)
        {
            AerialCamera->SetActive(true);
            ThirdPersonCamera->SetActive(false);
            PC->SetViewTarget(this, Params); // pawn calc camera picks the active cam
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
    if (!AICon) return;

    const bool  bDynamic = (PatrolCheckpoints.Num() > 0);
    const int32 Count = bDynamic ? PatrolCheckpoints.Num() : Waypoints.Num();
    if (Count <= 0) return;

    const int32 Next = (CurrentWPIndex + 1) % Count;
    if (Next == CurrentWPIndex) return; // single-point guard

    CurrentWPIndex = Next;
    IssueMoveToCurrentTarget();
}

void ASimulationRobotPawn::IssueMoveToCurrentTarget()
{
    if (!AICon) return;

    FAIMoveRequest Req;
    Req.SetUsePathfinding(true);
    Req.SetAllowPartialPath(false);
    Req.SetAcceptanceRadius(AcceptanceRadius);

    if (PatrolCheckpoints.Num() > 0) Req.SetGoalLocation(PatrolCheckpoints[CurrentWPIndex]);
    else if (Waypoints.Num() > 0)    Req.SetGoalActor(Waypoints[CurrentWPIndex]);

    AICon->MoveTo(Req);
}

// --- cameras ---
void ASimulationRobotPawn::SetAerialView(bool bUseAerial)
{
    bUsingAerialView = bUseAerial;

    if (auto* PC = Cast<APlayerController>(GetController()))
    {
        ThirdPersonCamera->SetActive(!bUsingAerialView);
        AerialCamera->SetActive(bUsingAerialView);

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

// --- checkpoints ---
void ASimulationRobotPawn::SetPatrolCheckpoints(const TArray<FVector>& Locs)
{
    PatrolCheckpoints = Locs;
    CurrentWPIndex = 0;
}

// --- screen → world ---
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

            // fallback to Z=0 plane if nothing hit
            if (!FMath::IsNearlyZero(WorldDir.Z))
            {
                const float T = -Start.Z / WorldDir.Z;
                if (T > 0.f) { WorldLocation = Start + (WorldDir * T); return true; }
            }
        }
    }
    return false;
}

// --- threats ---
void ASimulationRobotPawn::OnThreatBegin(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
    bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
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
            if (PC->ProjectWorldLocationToScreen(C[i], S, /*bPlayerViewportRelative*/ true))
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
