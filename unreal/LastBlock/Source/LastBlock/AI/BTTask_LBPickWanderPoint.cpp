#include "AI/BTTask_LBPickWanderPoint.h"
#include "Zombies/LBZombieAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_LBPickWanderPoint::UBTTask_LBPickWanderPoint()
{
	NodeName = TEXT("LB Pick Wander Point");
}

EBTNodeResult::Type UBTTask_LBPickWanderPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const ALBZombieAIController* AI = Cast<ALBZombieAIController>(OwnerComp.GetAIOwner());
	if (!AI) return EBTNodeResult::Failed;
	OwnerComp.GetBlackboardComponent()->SetValueAsVector(ALBZombieAIController::KeyMoveGoal, AI->PickWanderPoint());
	return EBTNodeResult::Succeeded;
}
