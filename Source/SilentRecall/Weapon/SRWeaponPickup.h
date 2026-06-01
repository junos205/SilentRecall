#pragma once

#include "CoreMinimal.h"
#include "Weapon/SRItemPickupBase.h" // 부모 상속 헤더
#include "Interface/ItemStateInterface.h"
#include "Data/SRCharacterData.h"
#include "SRWeaponPickup.generated.h"

UCLASS()
class SILENTRECALL_API ASRWeaponPickup : public ASRItemPickupBase, public IItemStateInterface
{
	GENERATED_BODY()

public:
	ASRWeaponPickup();

	virtual void SetDroppedAmmo_Implementation(int32 AmmoAmount) override { SavedAmmo = AmmoAmount; }

protected:
	// 부모의 획득 가상 함수를 가로채 실제 무기 인스턴스 스왑 및 처리
	virtual void OnPickedUp(class USRInventoryComponent* InventoryComp) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMesh;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	class USRWeaponDataAsset* ItemDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	int32 SavedAmmo = 30;
};