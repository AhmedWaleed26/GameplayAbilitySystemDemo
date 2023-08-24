// Copyright Epic Games, Inc. All Rights Reserved.

#include "GASPracticeCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Attributes/MyAttributeSet.h"
#include "GameplayAbility//PracticeGameplayAbility.h"
#include <Kismet/KismetMathLibrary.h>
#include "Math/UnrealMathUtility.h"
#include "InputMappingContext.h"
#include "AI/AIEnemyCharacter.h"
#include "Components/SphereComponent.h"


//////////////////////////////////////////////////////////////////////////
// AGASPracticeCharacter

AGASPracticeCharacter::AGASPracticeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	RightHandWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandWeapon"));
	RightHandWeapon->SetupAttachment(GetMesh(), FName("LHip_Weapon"));
	RightHandWeapon->SetCollisionProfileName(FName("Custom"));
	RightHandWeapon->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RightHandWeapon->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	RightHandWeapon->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	RightHandWeapon->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMyAttributeSet>("AttributeSet");

	GrappleCollider = CreateDefaultSubobject<USphereComponent>(TEXT("GrappleCollider"));
	GrappleCollider->InitSphereRadius(1000.f);
	GrappleCollider->SetupAttachment(RootComponent);
	GrappleCollider->SetRelativeLocation(FVector(400.f, 0.f, 0.f));
	GrappleCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GrappleCollider->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	GrappleCollider->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GrappleCollider->OnComponentBeginOverlap.AddDynamic(this, &AGASPracticeCharacter::OverlapGrapplePoint);
	GrappleCollider->OnComponentEndOverlap.AddDynamic(this, &AGASPracticeCharacter::OverlapEndGrapplePoint);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

UAbilitySystemComponent* AGASPracticeCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AGASPracticeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!AbilitySystemComponent)
		return;

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	InitializeAbilities();
	InitializeEffects();
}

void AGASPracticeCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (!AbilitySystemComponent)
		return;

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	BindInput();
	InitializeEffects();
}

void AGASPracticeCharacter::InitializeAbilities()
{
	//Give abilities, server only
	if (!HasAuthority() || !AbilitySystemComponent)
		return;

	for (TSubclassOf<UPracticeGameplayAbility>& Ability : DefaultAbilities)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, static_cast<int32>(Ability.GetDefaultObject()->AbilityInputID), this));
	}
}

void AGASPracticeCharacter::InitializeEffects()
{
	if (!AbilitySystemComponent)
		return;

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	for (TSubclassOf<UGameplayEffect>& Effect : DefaultEffects)
	{
		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(Effect, 1, EffectContext);
		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle GEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void AGASPracticeCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (!AbilitySystemComponent)
		return;

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AGASPracticeCharacter::OnHealthAttributeChanged);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AGASPracticeCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::Look);

		//Fire
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::OnFireAbility);

		//Attack
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::Attack);

		//Heavy Attack
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::HeavyAttack);

		//Defend
		EnhancedInputComponent->BindAction(DefendAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::Defend);

		//Target Lock
		EnhancedInputComponent->BindAction(TargetLockAction, ETriggerEvent::Triggered, this, &AGASPracticeCharacter::TargetLock);
	}

	BindInput();

}

void AGASPracticeCharacter::BindInput()
{
	if (bIsInputBound || !AbilitySystemComponent || !IsValid(InputComponent))
		return;

	FTopLevelAssetPath EnumAssetPath = FTopLevelAssetPath(FName("/Script/GASPractice"), FName("EAbilityInputID"));
	AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, FGameplayAbilityInputBinds(FString("Confirm"), FString("Cancel"), EnumAssetPath, static_cast<int32>(EAbilityInputID::Confirm), static_cast<int32>(EAbilityInputID::Cancel)));

	bIsInputBound = true;
}

void AGASPracticeCharacter::OnFireAbility(const FInputActionValue& Value)
{
	SendAbilityLocalInput(Value, static_cast<int32>(EAbilityInputID::FireAbility));
}

void AGASPracticeCharacter::SendAbilityLocalInput(const FInputActionValue& Value, int32 InputID)
{
	if (!AbilitySystemComponent)
		return;

	if (Value.Get<bool>())
	{
		AbilitySystemComponent->AbilityLocalInputPressed(InputID);
	}
	else
	{
		AbilitySystemComponent->AbilityLocalInputReleased(InputID);
	}
}

void AGASPracticeCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnHealthChanged(Data.OldValue, Data.NewValue);
}

void AGASPracticeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AGASPracticeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr && !isLocked)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AGASPracticeCharacter::Attack(const FInputActionValue& Value)
{
	SendAbilityLocalInput(Value, static_cast<int32>(EAbilityInputID::AttackAbility));
}

void AGASPracticeCharacter::HeavyAttack(const FInputActionInstance& Value)
{
	if (Value.GetElapsedTime() >= 0.5f) {
		SendAbilityLocalInput(Value.GetValue(), static_cast<int32>(EAbilityInputID::HeavyAttackAbility));
	}
}

void AGASPracticeCharacter::Defend(const FInputActionValue& Value)
{
	SendAbilityLocalInput(Value, static_cast<int32>(EAbilityInputID::DefendAbility));
}

void AGASPracticeCharacter::RemoveLock()
{
	isLocked = false;
	HitTarget = nullptr;
	bUseControllerRotationYaw = false;
	CameraBoom->bUsePawnControlRotation = true;
	NewCameraLocation = FVector(FollowCamera->GetRelativeLocation().X + 200.f, FollowCamera->GetRelativeLocation().Y, FollowCamera->GetRelativeLocation().Z);
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AGASPracticeCharacter::CreateLock(FHitResult OutHit)
{
	isLocked = true;
	HitTarget = OutHit.GetActor();
	bUseControllerRotationYaw = true;
	NewCameraRotation = FRotator(-25.f, CameraBoom->GetRelativeRotation().Roll, CameraBoom->GetRelativeRotation().Yaw);
	NewCameraLocation = FVector(FollowCamera->GetRelativeLocation().X - 200.f, FollowCamera->GetRelativeLocation().Y, FollowCamera->GetRelativeLocation().Z);
	CameraBoom->bUsePawnControlRotation = false;
	TargetedAI = static_cast<AAIEnemyCharacter*>(HitTarget);
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AGASPracticeCharacter::TargetLock(const FInputActionValue& Value)
{
	if (!isLocked)
	{
		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypesArray;
		ObjectTypesArray.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));
		TArray<AActor*, FDefaultAllocator> IgnoredActors;
		FHitResult OutHit;
		bool bHasHit = UKismetSystemLibrary::SphereTraceSingleForObjects(GetWorld(), GetActorLocation() + (FollowCamera->GetComponentToWorld().GetRotation().GetForwardVector() * 200.f), GetActorLocation() + (FollowCamera->GetComponentToWorld().GetRotation().GetForwardVector() * 1000.f), 125.f, ObjectTypesArray, false, IgnoredActors, EDrawDebugTrace::ForDuration, OutHit, true);
		if (bHasHit)
		{
			CreateLock(OutHit);
		}
	}
	else
	{
		RemoveLock();
	}
}

void AGASPracticeCharacter::OverlapGrapplePoint(class UPrimitiveComponent* Comp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TeleportLocation = OtherActor->GetActorLocation();
	Teleport = true;
}

void AGASPracticeCharacter::OverlapEndGrapplePoint(class UPrimitiveComponent* Comp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Teleport = false;
}


void AGASPracticeCharacter::Tick(float DeltaTime)
{
	if (isLocked && HitTarget)
	{
		// Always look at target
		FRotator PlayerRot = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), HitTarget->GetActorLocation());
		if (!GetController()->GetControlRotation().Equals(PlayerRot, 8.0)) 
		{
			GetController()->SetControlRotation(FMath::RInterpTo(GetController()->GetControlRotation(), PlayerRot, DeltaTime, 5.f));
		}
		// Rotate boom for better locked view
		if (!CameraBoom->GetRelativeRotation().Equals(NewCameraRotation, 0.5))
		{
			CameraBoom->SetRelativeRotation(FMath::RInterpTo(CameraBoom->GetRelativeRotation(), NewCameraRotation, DeltaTime, 5.f));
		}
		if (TargetedAI != nullptr)
		{
			if (TargetedAI->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Dead"))) || TargetedAI->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Dying"))))
			{
				RemoveLock();
			}
		}
	}
	// Move camera location for better view
	if (!FollowCamera->GetRelativeLocation().Equals(NewCameraLocation, 0.5))
	{
		FollowCamera->SetRelativeLocation(FMath::VInterpTo(FollowCamera->GetRelativeLocation(), NewCameraLocation, DeltaTime, 5.f));
	}
	if (Teleport)
	{
		if (GetActorLocation().Equals(TeleportLocation, 200.0)) {
			Teleport = false;
		}
		else {
			SetActorLocation(FMath::VInterpTo(GetActorLocation(), TeleportLocation, DeltaTime, 5.f));
		}
	}
}


