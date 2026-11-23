#include "AI/BTService_LBZombieSenses.h"
#include "Zombies/LBZombieAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTService_LBZombieSenses::UBTService_LBZombieSenses()
{
	NodeName = TEXT("LB Zombie Senses");
	// same rhythm as the browser version: think every .25-.4 s, with jitter so a horde doesn't sync up
	Interval = .3f;
	RandomDeviation = .07f;
	bNotifyTick = true;
}

void UBTService_LBZombieSenses::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	if (ALBZombieAIController* AI = Cast<ALBZombieAIController>(OwnerComp.GetAIOwner())) AI->UpdateSenses();
}
