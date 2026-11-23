#include "AI/BTTask_LBZombieAttack.h"
#include "Zombies/LBZombie.h"
#include "Zombies/LBZombieAIController.h"
#include "Core/LBTypes.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_LBZombieAttack::UBTTask_LBZombieAttack()
{
	NodeName = TEXT("LB Zombie Attack");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_LBZombieAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	ALBZombie* Z = AI ? Cast<ALBZombie>(AI->GetPawn()) : nullptr;
	AActor* Target = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(ALBZombieAIController::KeyTarget));
	if (!Z || !Target) return EBTNodeResult::Failed;
	if (FVector::Dist2D(Z->GetActorLocation(), Target->GetActorLocation()) > Reach * LB_TILE) return EBTNodeResult::Failed;
	if (!Z->CanAttack()) return EBTNodeResult::Failed;
	AI->StopMovement();
	Z->StartAttack(Target);
	return EBTNodeResult::InProgress;
}

void UBTTask_LBZombieAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	const ALBZombie* Z = AI ? Cast<ALBZombie>(AI->GetPawn()) : nullptr;
	if (!Z || Z->bDead) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }
	// a hit cancels the wind-up; either way the task ends when the swing is over
	if (Z->AttackWindup <= 0.f) FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
