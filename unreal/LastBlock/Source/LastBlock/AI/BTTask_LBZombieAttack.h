#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_LBZombieAttack.generated.h"

/** Swing at the Target key: waits through the wind-up, damage lands only if the player is still in reach */
UCLASS(meta = (DisplayName = "LB Zombie Attack"))
class LASTBLOCK_API UBTTask_LBZombieAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_LBZombieAttack();

	/** Start the swing only this close (tiles) */
	UPROPERTY(EditAnywhere, Category = "Attack") float Reach = .6f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
