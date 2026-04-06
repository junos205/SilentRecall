// Fill out your copyright notice in the Description page of Project Settings.


#include "SRInventoryComponent.h"
#include "Data/SRWeaponDataAsset.h"
#include "Weapon/SRWeaponInstance.h"
#include "GameplayAbilitySpecHandle.h"
#include "Interface/ItemStateInterface.h"


// Sets default values for this component's properties
USRInventoryComponent::USRInventoryComponent()
{
	
}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    if (SlotType == EWeaponSlot::None) return false;

    // 1. 이미 해당 슬롯에 무기가 있다면? 바닥으로 던져버립니다! (Drop & Throw)
    if (WeaponLoadout.Contains(SlotType) && SpawnedWeapons.Contains(SlotType))
    {
       AActor* OldWeapon = SpawnedWeapons[SlotType];
       USRWeaponInstance* OldInstance = WeaponLoadout[SlotType]; 

       if (OldWeapon && OldInstance)
       {
          if (OldWeapon->Implements<UItemStateInterface>())
          {
              IItemStateInterface::Execute_SetDroppedAmmo(OldWeapon, OldInstance->CurrentAmmoInMag);
          }

          OldWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); 
            
          if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(OldWeapon->GetRootComponent()))
          {
             RootComp->SetSimulatePhysics(true);
             RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
             
             // ⭐️ [추가] 던질 때: 무기 안의 모든 메쉬(스켈레탈/스태틱) 콜리전을 다시 켜줍니다!
             TArray<UMeshComponent*> Meshes;
             OldWeapon->GetComponents<UMeshComponent>(Meshes);
             for (UMeshComponent* Mesh : Meshes)
             {
                 Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                 
                 // 만약 무기의 RootComp가 아니라 Mesh 자체가 물리 연산을 해야 하는 구조라면 아래 주석을 해제하세요.
                 // Mesh->SetSimulatePhysics(true); 
             }

             if (AActor* OwnerActor = GetOwner())
             {
                 FVector ThrowDirection = OwnerActor->GetActorForwardVector() + FVector(0.0f, 0.0f, 0.5f);
                 ThrowDirection.Normalize(); 
                    
                 float ThrowForce = 100.0f; 
                 // 물리 연산을 적용하는 주체가 RootComp라면 여기에 임펄스를 줍니다.
                 RootComp->AddImpulse(ThrowDirection * ThrowForce, NAME_None, true); 
             }
          }

          if (SlotType == CurrentActiveSlot)
          {
              UnEquipWeapon(); 
              CurrentActiveSlot = EWeaponSlot::None; 
          }
       }
    }

    WeaponLoadout.Add(SlotType, NewInstance);

    // 3. 새 무기의 육신(액터) 처리 및 등에 숨기기(Holster)
    if (PickedUpWeaponActor)
    {
       if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(PickedUpWeaponActor->GetRootComponent()))
       {
          RootComp->SetSimulatePhysics(false);
          RootComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       }

       // ⭐️ [추가] 주울 때: 무기 안의 모든 메쉬 콜리전을 강제로 꺼버립니다! (카메라 충돌, 캐릭터 밀림 방지)
       TArray<UMeshComponent*> PickedMeshes;
       PickedUpWeaponActor->GetComponents<UMeshComponent>(PickedMeshes);
       for (UMeshComponent* Mesh : PickedMeshes)
       {
           Mesh->SetSimulatePhysics(false);
           Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       }

       FName HolsterSocket = (SlotType == EWeaponSlot::Melee) ? FName("HandGrip_R") : FName("HandGrip_R"); 
       
       if (USkeletalMeshComponent* OwnerMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
       {
           PickedUpWeaponActor->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
       }
        
       SpawnedWeapons.Add(SlotType, PickedUpWeaponActor);
    }

    // 4. 즉시 장착 연출
    EquipWeapon(SlotType);

    return true;
}

void USRInventoryComponent::EquipWeapon(EWeaponSlot SlotToEquip)
{
	if (CurrentActiveSlot == SlotToEquip || !SpawnedWeapons.Contains(SlotToEquip)) return;

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

	// ⭐️ 1. 기존 무기 숨기기 & 주입했던 스킬들 뺏기 (Remove Abilities)
	if (CurrentActiveSlot != EWeaponSlot::None)
	{
		// 등(Holster)으로 보내기
		AActor* CurrentWeapon = SpawnedWeapons[CurrentActiveSlot];
		FName HolsterSocket = FName("HolsterSocket");
		CurrentWeapon->AttachToComponent(GetOwner()->FindComponentByClass<USkeletalMeshComponent>(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
        
		// GAS 스킬 뺏기
		if (ASC)
		{
			for (FGameplayAbilitySpecHandle Handle : CurrentGrantedAbilityHandles)
			{
				ASC->ClearAbility(Handle); // 영수증을 보고 스킬을 삭제
			}
			CurrentGrantedAbilityHandles.Empty(); // 영수증 목록 초기화
		}
	}
	
	// GAS 스킬 주입!
	USRWeaponInstance* WeaponInstance = WeaponLoadout[SlotToEquip];
	AActor* WeaponToEquip = SpawnedWeapons[SlotToEquip]; 

	if (!WeaponInstance || !WeaponInstance->WeaponData || !WeaponToEquip || !ASC) return;

	// 손에 쥐여주기
	WeaponToEquip->AttachToComponent(GetOwner()->FindComponentByClass<USkeletalMeshComponent>(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("HandGrip_R"));

	// ⭐️ 2. GAS 스킬 주입! (인스턴스 안의 WeaponData를 열어서 스킬북을 읽습니다)
	for (const TTuple<EInputAction, TSubclassOf<UGameplayAbility>>& AbilityPair : WeaponInstance->WeaponData->GrantedAbilities)
	{
		EInputAction InputID = AbilityPair.Key;
		TSubclassOf<UGameplayAbility> AbilityClass = AbilityPair.Value;

		if (AbilityClass)
		{
			// 🎯 궁극의 페이로드 탑재!
			// 4번째 인수(SourceObject) 자리에 데이터 애셋이 아닌 'WeaponInstance'를 통째로 넣습니다!
			FGameplayAbilitySpec Spec(AbilityClass, 1, static_cast<int32>(InputID), WeaponInstance);
            
			FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			CurrentGrantedAbilityHandles.Add(Handle);
		}
	}

	CurrentActiveSlot = SlotToEquip;

}

void USRInventoryComponent::UnEquipWeapon()
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	
	if (CurrentActiveSlot == EWeaponSlot::None || !ASC) return;

	// 1. 뺏기: 이 무기를 쥐여줄 때 발급했던 GAS 스킬 영수증(Handle)들을 싹 다 취소합니다.
	for (const FGameplayAbilitySpecHandle& Handle : CurrentGrantedAbilityHandles)
	{
		ASC->ClearAbility(Handle);
	}
	// 영수증 목록을 깨끗하게 비웁니다.
	CurrentGrantedAbilityHandles.Empty(); 

	// 2. 숨기기: 손에 들고 있던 무기 액터를 다시 등(Holster)으로 보냅니다.
	if (SpawnedWeapons.Contains(CurrentActiveSlot))
	{
		AActor* WeaponToHide = SpawnedWeapons[CurrentActiveSlot];
		if (WeaponToHide)
		{
			// 근접 무기냐 원거리 무기냐에 따라 돌아갈 등짝 소켓을 결정합니다.
			FName HolsterSocket = (CurrentActiveSlot == EWeaponSlot::Melee) ? FName("Socket_Back_Melee") : FName("Socket_Back_Rifle");
            
			if (USkeletalMeshComponent* OwnerMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
			{
				// 무기를 손에서 떼서 등으로 찰칵! 붙입니다.
				WeaponToHide->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
			}
		}
	}
}


