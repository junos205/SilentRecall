#pragma once

#include "CoreMinimal.h"
#include "Weapon/SRItemPickupBase.h" // 부모 상속 헤더
#include "GameplayTagContainer.h"
#include "SRAmmoPickup.generated.h"

UCLASS()
class SILENTRECALL_API ASRAmmoPickup : public ASRItemPickupBase
{
	GENERATED_BODY()
    
public:
	ASRAmmoPickup();

protected:
	// 부모의 획득 가상 함수를 가로채 실제 탄약 지급 처리
	virtual void OnPickedUp(class USRInventoryComponent* InventoryComp) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* AmmoMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo Data")
	FGameplayTag AmmoTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo Data")
	int32 AmmoAmount = 30;
	

};