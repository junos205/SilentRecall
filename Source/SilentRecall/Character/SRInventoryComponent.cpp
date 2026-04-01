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
          // [데이터 역방향 복사] OldWeapon이 누군진 몰라도, 인터페이스가 있으면 남은 총알을 줘라!
          if (OldWeapon->Implements<UItemStateInterface>())
          {
              IItemStateInterface::Execute_SetDroppedAmmo(OldWeapon, OldInstance->CurrentAmmoInMag);
          }

          // 몸에서 떼어내기
          OldWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); 
            
          if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(OldWeapon->GetRootComponent()))
          {
             // 물리 켜기, 충돌 켜기 (바닥에 튕기게 하기 위함)
             RootComp->SetSimulatePhysics(true);
             RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

             // 던지는 로직 (대각선 위로 튕겨 나가게 설정)
             if (AActor* OwnerActor = GetOwner())
             {
                 FVector ThrowDirection = OwnerActor->GetActorForwardVector() + FVector(0.0f, 0.0f, 0.5f);
                 ThrowDirection.Normalize(); 
                    
                 float ThrowForce = 100.0f; 
                 RootComp->AddImpulse(ThrowDirection * ThrowForce, NAME_None, true); 
             }
          }

          // ⭐️ [치명적 버그 방지] 지금 내 손에 들고 있던 무기를 던진 거라면? 빈손으로 만들어라!
          if (SlotType == CurrentActiveSlot)
          {
              // 기존 무기가 부여했던 GAS 스킬(GA_Shoot, GA_Melee 등) 영수증을 모두 회수합니다.
              UnEquipWeapon(); 
              
              // 내 상태를 완벽한 '빈손'으로 업데이트합니다.
              CurrentActiveSlot = EWeaponSlot::None; 
          }
       }
    }

    // 2. 새 무기의 영혼(인스턴스)을 가방(TMap)에 등록합니다. (기존 데이터는 자동으로 덮어씌워짐)
    WeaponLoadout.Add(SlotType, NewInstance);

    // 3. 새 무기의 육신(액터) 처리 및 등에 숨기기(Holster)
    if (PickedUpWeaponActor)
    {
       if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(PickedUpWeaponActor->GetRootComponent()))
       {
          // 가방에 들어왔으니 물리 연산과 충돌을 완전히 끕니다.
          RootComp->SetSimulatePhysics(false);
          RootComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       }

       // 슬롯에 따라 붙일 등짝/허리 소켓 이름 결정 (프로젝트 세팅에 맞게 수정하세요)
       FName HolsterSocket = (SlotType == EWeaponSlot::Melee) ? FName("HandGrip_R") : FName("HandGrip_R"); 
       
       if (USkeletalMeshComponent* OwnerMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
       {
           // 액터를 플레이어의 등 소켓에 부착합니다.
           PickedUpWeaponActor->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
       }
        
       // 가방(무기 보관함)에 새 액터 포인터를 갱신합니다.
       SpawnedWeapons.Add(SlotType, PickedUpWeaponActor);
    }

    // 4. 모든 정리가 끝났으니, 주운 무기를 손에 쥐는(Equip) 연출을 즉시 실행합니다!
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


