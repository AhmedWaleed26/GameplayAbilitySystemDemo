// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GASPractice/GASPractice.h"
#include "PracticeGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class GASPRACTICE_API UPracticeGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	EAbilityInputID AbilityInputID { EAbilityInputID::None };
};
