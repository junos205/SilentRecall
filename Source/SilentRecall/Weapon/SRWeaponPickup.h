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

	FName GetEquipSocketName() const { return EquipSocketName; }

	FName GetHolsterSocketName() const { return HolsterSocketName; }
	
	// ⭐️ 3인칭(전신) 메쉬에 덮어씌울 상체 무기 애니메이션 레이어
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> TP_AnimLayerClass;

	// ⭐️ 1인칭(팔) 메쉬에 덮어씌울 무기 애니메이션 레이어
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> FP_AnimLayerClass;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMesh; // 바닥에 보일 모델링
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	class USRWeaponDataAsset* ItemDataAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sockets")
	FName EquipSocketName = FName("HandGrip_R");

	// 등에 맬 때 쓸 소켓 이름 (예: "Holster_Back", "Holster_Hip")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sockets")
	FName HolsterSocketName = FName("HolsterSocket");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	int32 SavedAmmo = 30;
};
