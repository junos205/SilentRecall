// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "Interface/ItemStateInterface.h"
#include "Data/SRCharacterData.h"
#include "SRWeaponPickup.generated.h"

UCLASS()
class SILENTRECALL_API ASRWeaponPickup : public AActor, public IInteractableInterface, public IItemStateInterface
{
	GENERATED_BODY()

public:	
	ASRWeaponPickup();

	// ⭐️ 인터페이스 함수 구현부 (BlueprintNativeEvent는 뒤에 _Implementation이 붙습니다)
	virtual void Interact_Implementation(AActor* Interactor) override;

	virtual void SetDroppedAmmo_Implementation(int32 AmmoAmount) override
	{
		SavedAmmo = AmmoAmount;
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMesh; // 바닥에 보일 모델링
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	class USRWeaponDataAsset* ItemDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	int32 SavedAmmo = 30;
};
