// Fill out your copyright notice in the Description page of Project Settings.


#include "SRBaseCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "SRInventoryComponent.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "Data/SRWeaponDataAsset.h"

// Sets default values
ASRBaseCharacter::ASRBaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ASC
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	// 이 액터가 네트워크에서 복제되어야 함을 보장합니다.
	SetReplicates(true);

	// 복제 모드 설정
	ASC->SetIsReplicated(true);
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	AttributeSet = CreateDefaultSubobject<USRDefaultAttributeSet>(TEXT("AttributeSet"));

	InventoryComponent = CreateDefaultSubobject<USRInventoryComponent>(TEXT("WeaponComponent"));

	if (InventoryComponent)
	{
		UE_LOG(LogTemp, Display, TEXT("[BaseCharacter] InventoryComponent is Valid"));
		InventoryComponent->OnWeaponChanged.AddDynamic(this, &ASRBaseCharacter::HandleWeaponChanged);
	}
	
	// Mesh
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetCollisionProfileName(TEXT("NoCollision")); 

	// static ConstructorHelpers::FObjectFinder<USkeletalMesh> CharacterMeshRef(TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple'"));
	// if (CharacterMeshRef.Object)
	// {
	// 	GetMesh()->SetSkeletalMesh(CharacterMeshRef.Object);
	// }
	//
	// static ConstructorHelpers::FClassFinder<UAnimInstance> AnimInstanceClassRef(TEXT("/Game/Variant_Shooter/Anims/ABP_Unarmed.ABP_Unarmed_C"));
	// if (AnimInstanceClassRef.Class)
	// {
	// 	GetMesh()->SetAnimInstanceClass(AnimInstanceClassRef.Class);
	// }
	
}

UAbilitySystemComponent* ASRBaseCharacter::GetAbilitySystemComponent() const
{
	return ASC;
}


void ASRBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (ASC && HasAuthority())
	{
		for (TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
		{
			// ⭐️ [필수 추가] Ability가 비어있지 않은지 반드시 검사해야 합니다!
			if (Ability)
			{
				FGameplayAbilitySpec AbilitySpec(Ability);
				ASC->GiveAbility(AbilitySpec);
			}
		}
	}
}

void ASRBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ASRBaseCharacter::HandleWeaponChanged(class USRWeaponDataAsset* NewWeaponData)
{
	UE_LOG(LogTemp, Error, TEXT("[3. Character] HandleWeaponChanged 호출됨!"));

	// 2. 엔진의 준비 상태(Null 검사) 아주 상세하게 출력!
	FString LayerName = NewWeaponData && NewWeaponData->TP_AnimLayerClass ? NewWeaponData->TP_AnimLayerClass->GetName() : TEXT("NULL");
	bool bHasMesh = (GetMesh() != nullptr);
	bool bHasAnimInstance = bHasMesh ? (GetMesh()->GetAnimInstance() != nullptr) : false;

	UE_LOG(LogTemp, Error, TEXT("[3. Character 상세] 레이어 클래스: %s | Mesh 준비됨: %d | AnimInstance 준비됨: %d"), 
		*LayerName, bHasMesh, bHasAnimInstance);
	// 1. 기존 3P 레이어가 있다면 해제
	if (CurrentTPLayer && GetMesh()) 
	{
		GetMesh()->UnlinkAnimClassLayers(CurrentTPLayer);
		CurrentTPLayer = nullptr;
	}

	// 2. 새 무기 데이터가 있고 3P 레이어 클래스가 존재한다면 연결
	if (NewWeaponData && NewWeaponData->TP_AnimLayerClass && GetMesh())
	{
		GetMesh()->LinkAnimClassLayers(NewWeaponData->TP_AnimLayerClass);
		CurrentTPLayer = NewWeaponData->TP_AnimLayerClass; 
	}
}

void ASRBaseCharacter::AttachWeaponToHolster(AActor* WeaponActor, FName EquipSocketName)
{
	if (!WeaponActor) return;

	WeaponActor->SetOwner(this);
	WeaponActor->SetActorHiddenInGame(false); 
	WeaponActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocketName);
}

void ASRBaseCharacter::AttachWeaponToHands(AActor* WeaponActor, FName EquipSocketName)
{
	// ⭐️ [로직 복구] 3P 메쉬에 무기를 붙여주는 로직을 채워주세요!
	if (!WeaponActor) return;

	WeaponActor->SetOwner(this);
	WeaponActor->SetActorHiddenInGame(false); 
	WeaponActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocketName);
}

void ASRBaseCharacter::PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly)
{
	if (MontageToPlay && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(MontageToPlay);
	}
}



