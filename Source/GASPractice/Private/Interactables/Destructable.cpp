// Fill out your copyright notice in the Description page of Project Settings.


#include "Interactables/Destructable.h"
#include "AbilitySystemComponent.h"
#include "Attributes//MyAttributeSet.h"

// Sets default values
ADestructable::ADestructable()
{
	StaticMeshComp = CreateAbstractDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetSimulatePhysics(true);
	StaticMeshComp->SetCollisionObjectType(ECC_PhysicsBody);
	RootComponent = StaticMeshComp;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateAbstractDefaultSubobject<UMyAttributeSet>("AttributeSet");
}

// Called when the game starts or when spawned
void ADestructable::BeginPlay()
{
	Super::BeginPlay();
	
	if (!AbilitySystemComponent)
		return;

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &ADestructable::OnHealthAttributeChanged);
}

UAbilitySystemComponent* ADestructable::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADestructable::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnHealthChanged(Data.OldValue, Data.NewValue);
}
