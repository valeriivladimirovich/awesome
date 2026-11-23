#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_LBZombieSenses.generated.h"

/** Put on the root Selector: sight, memory of the player and stuck checks (writes State / Target / MoveGoal) */
UCLASS(meta = (DisplayName = "LB Zombie Senses"))
class LASTBLOCK_API UBTService_LBZombieSenses : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_LBZombieSenses();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
