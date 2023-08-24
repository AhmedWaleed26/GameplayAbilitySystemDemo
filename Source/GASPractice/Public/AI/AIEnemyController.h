// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "AIEnemyController.generated.h"


UCLASS()
class GASPRACTICE_API AAIEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	AAIEnemyController();

	UPROPERTY(VisibleAnywhere, Category = AI)
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent = nullptr;
	TObjectPtr<class UAISenseConfig_Sight> AISenseConfigSight = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AI)
	TObjectPtr<UBehaviorTree> BehaviourTree = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = AI)
	UBlackboardData* BlackboardToUse;

	UPROPERTY()
	UBlackboardComponent* BB;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTargetPerceptionUpdated_Delegate(AActor* Actor, FAIStimulus Stimulus);

protected:
	UFUNCTION()
	void HandleSightSense(AActor* Actor, FAIStimulus Stimulus);
};
