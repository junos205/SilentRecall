// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Data/SRCharacterData.h"
#include "SRInventoryComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SILENTRECALL_API USRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USRInventoryComponent();

	// ⭐️ 무기 습득: 특정 슬롯에 무기를 넣습니다.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, class AActor* PickedUpWeaponActor);
	// ⭐️ 무기 스왑: 해당 슬롯의 무기로 교체합니다.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void EquipWeapon(EWeaponSlot SlotToEquip);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UnEquipWeapon();

protected:
	// ⭐️ 핵심 자료구조: Key는 슬롯 종류, Value는 무기 데이터
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory")
	TMap<EWeaponSlot, USRWeaponInstance*> WeaponLoadout;
	
	UPROPERTY()
	TMap<EWeaponSlot, class AActor*> SpawnedWeapons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	EWeaponSlot CurrentActiveSlot = EWeaponSlot::None;

	UPROPERTY()
	class AActor* CurrentWeaponActor;

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> CurrentGrantedAbilityHandles;
};
