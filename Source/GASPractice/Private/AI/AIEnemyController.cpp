// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AIEnemyController.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

AAIEnemyController::AAIEnemyController()
{
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("PerceptionComponent");

	AISenseConfigSight = CreateDefaultSubobject<UAISenseConfig_Sight>("SenseSight");
	AISenseConfigSight->DetectionByAffiliation.bDetectEnemies = true;
	AISenseConfigSight->DetectionByAffiliation.bDetectFriendlies = true;
	AISenseConfigSight->DetectionByAffiliation.bDetectNeutrals = true;

	AIPerceptionComponent->ConfigureSense(*AISenseConfigSight);

}

void AAIEnemyController::BeginPlay()
{
	Super::BeginPlay();
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AAIEnemyController::OnTargetPerceptionUpdated_Delegate);

	if (!ensure(BlackboardToUse)) { return; } // We ensure that pointer isn't null
	UseBlackboard(BlackboardToUse, BB);

	if (BehaviourTree)
	{
		RunBehaviorTree(BehaviourTree);
	}
}

void AAIEnemyController::OnTargetPerceptionUpdated_Delegate(AActor* Actor, FAIStimulus Stimulus)
{
	HandleSightSense(Actor, Stimulus);
}

void AAIEnemyController::HandleSightSense(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.WasSuccessfullySensed())
	{
		if (Actor == UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
		{
			BB->SetValueAsObject("TargetActor", Actor);
		}
		else
		{
			BB->ClearValue("TargetActor");
		}
	}
	else
	{
		return;
	}
}
