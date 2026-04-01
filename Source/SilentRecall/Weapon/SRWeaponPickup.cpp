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
	if (!Interactor || !ItemDataAsset) return; // ⭐️ 데이터 애셋이 없으면 줍기 취소!

	USRInventoryComponent* InventoryComp = Interactor->FindComponentByClass<USRInventoryComponent>();

	if (InventoryComp)
	{
		UE_LOG(LogTemp, Log, TEXT("[WeaponPickup] Inventory comp is Valid..."));
		
		USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComp);
		NewInstance->InitializeInstance(ItemDataAsset, SavedAmmo);

		// 🎯 유저님의 피드백이 적용된 마법의 1줄! 
		// 픽업 액터의 변수가 아니라, '데이터 애셋 원본'에 적힌 슬롯을 읽어옵니다.
		EWeaponSlot SlotToUse = ItemDataAsset->WeaponSlotType;

		// "가방아, 이 무기는 원본 데이터를 보니까 'SlotToUse(근접/원거리)' 래! 그 칸에 넣어줘!"
		bool bSuccess = InventoryComp->AddWeapon(SlotToUse, NewInstance, this);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponPickup] 무기 획득: %s, 장착 슬롯: %d"), *ItemDataAsset->WeaponName.ToString(), (int32)SlotToUse);
		}
		else
		{
			NewInstance->ConditionalBeginDestroy();
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[WeaponPickup] No Inventory comp found..."));
	}
}