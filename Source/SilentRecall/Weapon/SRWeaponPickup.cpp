#include "SRWeaponPickup.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/SRInventoryComponent.h"
#include "SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

ASRWeaponPickup::ASRWeaponPickup()
{
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    
	// 🌟 [중요] 무기 메쉬 또한 부모의 'VisualRoot' 아래로 편입시켜 둥둥 뜨게 유도합니다.
	WeaponMesh->SetupAttachment(VisualRoot);
    
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASRWeaponPickup::OnPickedUp(USRInventoryComponent* InventoryComp)
{
	if (!ItemDataAsset) return;

	EWeaponSlot SlotToUse = ItemDataAsset->WeaponSlotType;
        
	USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComp);
	NewInstance->InitializeInstance(ItemDataAsset, SavedAmmo);

	// 무기 추가 (성공 시 인벤토리가 내부적으로 이전 무기를 바닥에 드랍 액터로 스폰시킴)
	bool bSuccess = InventoryComp->AddWeapon(SlotToUse, NewInstance, this);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponPickup] Auto-Overlapped %s with %d ammo."), *ItemDataAsset->WeaponName.ToString(), SavedAmmo);
        
		// 인벤토리 장착 처리가 성공했으므로 바닥 아이템 액터는 완전 파괴
		Destroy();
	}
	else
	{
		NewInstance->ConditionalBeginDestroy();
	}
}