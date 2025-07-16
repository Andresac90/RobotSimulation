#include "RobotAIController.h"
#include "NavigationSystem.h"

ARobotAIController::ARobotAIController()
{
    bAttachToPawn = true;
    bStartAILogicOnPossess = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ARobotAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    UE_LOG(LogTemp, Log, TEXT("RobotAIController: Possessed Pawn %s"), *InPawn->GetName());
}
