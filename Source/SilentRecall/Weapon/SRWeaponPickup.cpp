// Fill out your copyright notice in the Description page of Project Settings.


#include "SRWeaponPickup.h"

#include "SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

#include "Character/SRInventoryComponent.h"


ASRWeaponPickup::ASRWeaponPickup()
{
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
    
	// 바닥에 떨어져야 하니 물리 켜기 (스켈레탈 메쉬도 물리 적용이 완벽하게 됩니다!)
	WeaponMesh->SetSimulatePhysics(true);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void ASRWeaponPickup::Interact_Implementation(AActor* Interactor)
{
	if (!Interactor || !ItemDataAsset) return; 

	USRInventoryComponent* InventoryComp = Interactor->FindComponentByClass<USRInventoryComponent>();

	if (InventoryComp)
	{
		EWeaponSlot SlotToUse = ItemDataAsset->WeaponSlotType;

		// ❌ 이전에 추가했던 "같은 무기인지 확인하고 총알만 흡수하는 로직(ExistingWeapon 확인)"을 완전히 삭제했습니다!
		// 묻지도 따지지도 않고 바로 새 인스턴스를 만들어서 줍습니다.
        
		USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComp);
		NewInstance->InitializeInstance(ItemDataAsset, SavedAmmo);

		// 인벤토리로 넘기면, 인벤토리가 알아서 기존 무기를 바닥에 뱉어낼 것입니다.
		bool bSuccess = InventoryComp->AddWeapon(SlotToUse, NewInstance, this);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponPickup] Picked up %s with %d ammo."), *ItemDataAsset->WeaponName.ToString(), SavedAmmo);
		}
		else
		{
			NewInstance->ConditionalBeginDestroy();
		}
	}
}