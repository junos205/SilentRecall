#pragma once

#include "CoreMinimal.h"
#include "Weapon/SRItemPickupBase.h"
#include "GameplayTagContainer.h"
#include "SRAmmoPickup.generated.h"

UCLASS()
class SILENTRECALL_API ASRAmmoPickup : public ASRItemPickupBase
{
	GENERATED_BODY()
    
public:
	ASRAmmoPickup();

protected:
	virtual void OnPickedUp(class USRInventoryComponent* InventoryComp) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* AmmoMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo Data")
	FGameplayTag AmmoTypeTag;

	// 🌟 [수술 완료] '낱발'이 아니라 '탄창 1통'을 의미하도록 변수명과 기본값(1)을 교체!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo Data", meta=(ToolTip="획득 시 추가될 탄창 통의 개수입니다."))
	int32 MagazineAmount = 1; 
};