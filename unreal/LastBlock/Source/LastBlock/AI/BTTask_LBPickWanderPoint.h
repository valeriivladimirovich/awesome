#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_LBPickWanderPoint.generated.h"

/** Writes a random reachable point within 3 tiles into MoveGoal; follow with Move To (MoveGoal) and a Wait */
UCLASS(meta = (DisplayName = "LB Pick Wander Point"))
class LASTBLOCK_API UBTTask_LBPickWanderPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_LBPickWanderPoint();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
