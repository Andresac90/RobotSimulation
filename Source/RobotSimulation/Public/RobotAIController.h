#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RobotAIController.generated.h"

/**
 * Basic AIController for the robot
 */
UCLASS()
class ROBOTSIMULATION_API ARobotAIController : public AAIController
{
    GENERATED_BODY()

public:
    ARobotAIController();

protected:
    virtual void OnPossess(APawn* InPawn) override;
};
